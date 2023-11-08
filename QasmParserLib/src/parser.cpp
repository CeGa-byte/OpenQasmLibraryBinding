//
// Created by Cedric Gaberle on 25.03.23.
//

#include "parser.h"
#include "fmt/core.h"

#include <omp.h>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <execution>
#include <mutex>


void qasmparser::Parser::errorCheck(std::string& str, float& coef, long long& param) const {
    if (str.empty())
        throw std::invalid_argument("No operator provided!");
    else if (str.length() != numberQubits)
        throw std::invalid_argument("Non-matching length of string representation!");
    else if (coef == 0)
        throw std::invalid_argument("Zero coefficient!");
    else if (param < 0)
        throw std::invalid_argument("Negative parameter!");
    else if (param > std::numeric_limits<unsigned long>::max())
        throw std::invalid_argument("Parameter out of bound!");
}

void qasmparser::Parser::printError(const std::string &errMessage, unsigned long &idx) {
    std::cerr << "Error! At line " << idx << std::endl;
    std::cerr << errMessage << std::endl;
    std::exit(EXIT_FAILURE);
}

void qasmparser::Parser::readLines(const std::string& filename) {
    std::ifstream inFile(filename);

    if (inFile.is_open()){
        std::string line;
        unsigned long lineIdx = 0;

        while (getline(inFile, line)){
            lineIdx += 1;
            Parser::QuantumOperator qop;
            std::string strRep; float coef; unsigned long param;  // Operator parameters
            long long checkParam;

            std::istringstream is (line);
            if (!(is >> strRep >> coef >> checkParam)){
                inFile.close();
                printError(std::string("Wrong format!"), lineIdx);
            }

            // Set number of qubits corresponding to qubits in first operator (must be equal for all operators)
            if (lineIdx == 1)
                numberQubits = strRep.length();

            // Error checking on input line operator
            try {errorCheck(strRep, coef, checkParam);}
            catch (const std::invalid_argument& strException) {
                inFile.close();
                printError(strException.what(), lineIdx);
            }
            catch (...) {
                inFile.close();
                printError(std::string("Unknown Error!"), lineIdx);
            }

            param = checkParam;

            // Set independent parameter if provided is 0
            if (param == 0)
                param = lineIdx;

            // Store each line in QuantumOperator struct and push into operators vector
            qop.index = lineIdx; qop.strRep = strRep; qop.coef = coef; qop.param = param;
            operators.emplace_back(qop);
        }
    }
    inFile.close();
}

std::vector< std::vector<unsigned long> > qasmparser::Parser::parseStrInt(std::string &in) {
    std::vector<unsigned long> xVec, yVec, zVec;
    std::vector< std::vector<unsigned long> > intRep;
    intRep.reserve(3);

    // Check for Pauli characters in string representation
    for (auto ch = in.begin(); ch != in.end(); ch++) {
        switch (*ch) {
            case 'I':
                break;
            case 'X': {
                unsigned long i = std::distance(in.begin(), ch) + 1;  // Index of X character
                xVec.emplace_back(i);
                break;
            }
            case 'Y': {
                unsigned long i = std::distance(in.begin(), ch) + 1;  // Index of Y character
                yVec.emplace_back(i);
                break;
            }
            case 'Z': {
                unsigned long i = std::distance(in.begin(), ch) + 1;  // Index of Z character
                zVec.emplace_back(i);
                break;
            }
            default:
                throw std::invalid_argument("Unsupported character instruction!");
        }
    }

    intRep.emplace_back(xVec); intRep.emplace_back(yVec); intRep.emplace_back(zVec);
    return intRep;
}

std::string qasmparser::Parser::parseOpToQasm(QuantumOperator& qop) {
    // Parameterised rotation in Z basis. By definition this rotation is done on the last used qubit by the operator
    const auto lastUsed = std::max(
            {qop.intOp[0].empty() ? 0 : *std::max_element(qop.intOp[0].begin(), qop.intOp[0].end()),
             qop.intOp[1].empty() ? 0 : *std::max_element(qop.intOp[1].begin(), qop.intOp[1].end()),
             qop.intOp[2].empty() ? 0 : *std::max_element(qop.intOp[2].begin(), qop.intOp[2].end())}
    );

    std::string qasmOp = fmt::format("rz({}{}*$[{}]) q[{}];\n", mup, qop.coef, qop.param, lastUsed - 1);
    std::string beforeLast, afterLast;

    // Go through vector entries indicating pauli matrix on qubits at these indices and therefore rotations in
    // corresponding basis. First vector corresponds to Pauli-X, second to Pauli-Y, third and last to Pauli-Z.
    for (const auto& vec : qop.intOp) {
        switch (std::distance(qop.intOp.begin(), std::find(qop.intOp.begin(), qop.intOp.end(), vec))) {
            case 0:
                for (auto qubitIdx: vec) {
                    std::string before = fmt::format("ry(pi/2) q[{}];\n", qubitIdx - 1);
                    std::string after = fmt::format("ry(-pi/2) q[{}];\n", qubitIdx - 1);

                    if (qubitIdx == lastUsed) {
                        beforeLast = before;
                        afterLast = after;
                        continue;
                    }

                    before += fmt::format("cx q[{}], q[{}];\n", qubitIdx - 1, lastUsed - 1);
                    after.insert(0, fmt::format("cx q[{}], q[{}];\n", qubitIdx - 1, lastUsed - 1));

                    qasmOp.insert(0, before);
                    qasmOp += after;
                }
                break;

            case 1:
                for (auto qubitIdx: vec) {
                    // Rotation in x basis by -0.5 pi before and +0.5 pi afterwards
                    std::string before = fmt::format("rx(-pi/2) q[{}];\n", qubitIdx - 1);
                    std::string after = fmt::format("rx(pi/2) q[{}];\n", qubitIdx - 1);

                    if (qubitIdx == lastUsed) {
                        afterLast = after;
                        beforeLast = before;
                        continue;
                    }
                    // If operation is performed on last qubit used by operator, no CNOTs are needed
                    before += fmt::format("cx q[{}], q[{}];\n", qubitIdx - 1, lastUsed - 1);
                    after.insert(0, fmt::format("cx q[{}], q[{}];\n", qubitIdx - 1, lastUsed - 1));

                    qasmOp.insert(0, before);
                    qasmOp += after;
                }
                break;

            case 2:
                for (auto qubitIdx: vec) {
                    // Z vector, indicating Pauli Z operations. No rotation since we are in Z basis already
                    if (qubitIdx != lastUsed) {
                        std::string before = fmt::format("cx q[{}], q[{}];\n", qubitIdx - 1, lastUsed - 1);
                        std::string after = fmt::format("cx q[{}], q[{}];\n", qubitIdx - 1, lastUsed - 1);

                        qasmOp.insert(0, before);
                        qasmOp += after;
                    }
                }
                break;

            default:
                // To many vectors, therefore to many pauli operations, only 3 possible (Pauli-X, Pauli-Y, Pauli-Z)
                std::exit(EXIT_FAILURE);
        }
    }
    qasmOp.insert(0, beforeLast);
    qasmOp += afterLast;

    return qasmOp.insert(0, fmt::format("\n// New operator from line {}\n", qop.index));
}

std::string qasmparser::parseCircuitSeq(const std::string& inFilename,
                                     const int version,
                                     const std::optional<std::string>& outFilename,
                                     const std::optional<float>& multiplier) {
    Parser parser;
    std::string qasm;
    std::map<unsigned long, std::string> qasmOperators;
    if (multiplier.has_value())
        parser.mup = std::to_string(multiplier.value()) + "*";
    std::mutex parseOpMtx;

    // Read lines into `operators` vector
    parser.readLines(inFilename);

    // For each operator: parse string into integer representation, parse into OpenQASM, and store in `qasmOperators`
    std::for_each(std::execution::par, parser.operators.begin(), parser.operators.end(),
                  [&](Parser::QuantumOperator op) {
    //for (auto& op: parser.operators) {
        try {
            op.intOp = parser.parseStrInt(op.strRep);
        }
        catch (const std::invalid_argument& exception) {
            parser.printError(exception.what(), op.index);
        }

        std::string qasmOp = parser.parseOpToQasm(op);

        std::lock_guard<std::mutex> guard(parseOpMtx);
        qasmOperators[op.index] = qasmOp;
    });

    // OpenQASM version specific header
    if (version == 3) {
        qasm.insert(0, fmt::format("OPENQASM 3.0;\n"
                                   "include \"stdgates.inc\";\n"
                                   "qubit[{0}] q;\n"  // Qubit register of size `numberQubits`
                                   "bit[{0}] c;\n",   // Classical bit register of same size
                                   parser.numberQubits)
        );
    }
    else {
        qasm.insert(0, fmt::format("OPENQASM 2.0;\n"
                                   "include \"qelib1.inc\";\n"
                                   "qreg q[{0}];\n"   // Qubit register of size `numberQubits`
                                   "creg c[{0}];\n",  // Classical bit register of same size
                                   parser.numberQubits)
        );
    }

    // Write out OpenQASM representations of operators stored in `qasmOperators`
    if (!outFilename) {
        for (const auto& [idx, op] : qasmOperators) {
            qasm += op;
        }
        return qasm;
    }

    std::ofstream outFile (outFilename.value());
    if (outFile.is_open()) {
        outFile << qasm;
        for (const auto& [idx, op] : qasmOperators) {
            qasm += op;
            outFile << op;
        }
        outFile.close();
    }

    return qasm;
}

std::string qasmparser::parseCircuit(const std::string &inFilename, const bool useOpenMP, const int version,
                                     const std::optional<std::string> &outFilename,
                                     const std::optional<float> &multiplier) {
    Parser p;
    std::map<unsigned long, std::string> qasmOperators;
    std::string qasm;
    p.openmp = useOpenMP;

    if (multiplier.has_value())
        p.mup = std::to_string(multiplier.value()) + "*";

    std::mutex parseOpMtx;

    // Read lines into `operators` vector
    p.readLines(inFilename);

    if (useOpenMP) {
        #pragma omp parallel for default(none) shared(p, version, qasmOperators)
        for (auto &op: p.operators) {
            try {
                op.intOp = p.parseStrInt(op.strRep);
            }
            catch (const std::invalid_argument &exception) {
                #pragma omp critical (print)
                p.printError(exception.what(), op.index);
            }

            std::string qasmOp;
            qasmOp = p.parseOpToQasm(op);
            #pragma omp critical (qasmOperator)
            qasmOperators[op.index] = qasmOp;
        }
    }

    else {
        // For each operator: parse string into integer representation, parse into OpenQASM, and store in `qasmOperators`
        std::for_each(std::execution::par, p.operators.begin(), p.operators.end(),
                      [&](Parser::QuantumOperator op) {
                          //for (auto& op: parser.operators) {
                          try {
                              op.intOp = p.parseStrInt(op.strRep);
                          }
                          catch (const std::invalid_argument &exception) {
                              p.printError(exception.what(), op.index);
                          }

                          std::string qasmOp;
                          qasmOp = p.parseOpToQasm(op);

                          std::lock_guard<std::mutex> guard(parseOpMtx);
                          qasmOperators[op.index] = qasmOp;
                      });
    }

    // OpenQASM version specific header
    if (version == 3) {
        qasm.insert(0, fmt::format("OPENQASM 3.0;\n"
                                   "include \"stdgates.inc\";\n"
                                   "qubit[{0}] q;\n"  // Qubit register of size `numberQubits`
                                   "bit[{0}] c;\n",   // Classical bit register of same size
                                   p.numberQubits)
        );
    }
    else {
        qasm.insert(0, fmt::format("OPENQASM 2.0;\n"
                                   "include \"qelib1.inc\";\n"
                                   "qreg q[{0}];\n"   // Qubit register of size `numberQubits`
                                   "creg c[{0}];\n",  // Classical bit register of same size
                                   p.numberQubits)
        );
    }

    for (const auto& [idx, op] : qasmOperators) {
        qasm += op;
    }

    // Write out OpenQASM representations of operators stored in `qasmOperators`
    if (!outFilename) {
        for (const auto& [idx, op] : qasmOperators) {
            qasm += op;
        }
        return qasm;
    }

    std::ofstream outFile (outFilename.value());
    if (outFile.is_open()) {
        outFile << qasm;
        for (const auto& [idx, op] : qasmOperators) {
            qasm += op;
            outFile << op;
        }
        outFile.close();
    }

    return qasm;
}

int qasmparser::test(int a, int b){
    return a + b;
}
