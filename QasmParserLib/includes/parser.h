//
// Created by Cedric Gaberle on 25.03.23.
//

#ifndef QASM_PARSER_PARSER_H
#define QASM_PARSER_PARSER_H

#include <sstream>
#include <vector>
#include <map>
#include <optional>


namespace qasmparser {
    /**
     * Implementation of a Quantum Parser Class. Holds operators, coefficients and parameters of the input file in instance
     * of subclass QuantumOperator. Implements parsing functionality to parse into OpenQASM Standard, version specified in
     * variable.
     */
    class Parser {
    private:
        /**
         * Quantum operator struct holding information about operator, namely the index of occurrence in the input file,
         * gates working on which qubits, the coefficients, and the parameter. The parameter is used to indicate dependent
         * and independent operators by having the same (dependent) or different parameters (independent).
         */
        struct QuantumOperator {
            unsigned long index;                             // Index of operator in input (order of operators)
            std::string strRep;                              // String representation of operator
            std::vector<std::vector<unsigned long> > intOp;  // Operator in integer representation stored in vector of vectors
            float coef;                                      // Coefficient of operator
            unsigned long param;                             // Parameter indicating dependencies
        };

        unsigned long numberQubits;                          // Must equal length of operators in string representation
        std::string mup = "0.5*";                            // Float value to multiply operations as string
        std::vector<QuantumOperator> operators;              // Vector holding all operators as Quantum Operator struct
        bool openmp;                                         // Specify to use OpenMP parallelism

        /**
         * Parse string representation of input into shorter integer representation describing operator. Integer
         * representation of the form: vectors for each basis holding indices of corresponding character, meaning operation
         * in this particular basis takes place on qubit number index.
         * @param in : Representation of input operator in string format, e.g. IIIIXIY
         * @return Vector of three vectors, each Pauli operation, containing indices of rotations
         */
        static std::vector<std::vector<unsigned long> > parseStrInt(std::string &in);

        /**
         * Check input for errors. Throw error if string representations have different lengths, the coefficient has a value
         * of zero, or the parameter has a value of less than one. Different string length correspond to different numbers
         * of qubits, which is not possible in a single circuit. A coefficient of zero would let the matrix collaps to zero
         * and therefore would be useless. The parameter, indicating dependencies of operators here, must be in range
         * (1, number of operators) by definition.
         * @param str String representation of the operator, e.g. IIIIXIY.
         * @param coef Coefficient of operator.
         * @param param Parameter of operator.
         */
        void errorCheck(std::string &str, float &coef, long long &param) const;

        /**
         * Print error message and line of occurrence.
         * @param errMessage Error message to print.
         * @param idx Line in input file of error occurrence.
         */
        static void printError(const std::string &errMessage, unsigned long &idx);

        /**
         * Read lines of the input file, perform error checking, convert representation of operators and store in quantum
         * operator instances. Push all operator instances in operators member.
         * @param filename Path to the input file.
         */
        void readLines(const std::string &filename);

        /**
         * Parse QuantumOperator instance operator into OpenQASM representation. Pauli-X and -Y operations are performed
         * using rotations along the corresponding basis.
         * @param qubitIdx QuantumOperator instance holding index, parameter, coefficient, and integer representation
         * @return String representation of operator in QASM version 2
         */
        std::string parseOpToQasm(QuantumOperator &qubitIdx);

    public:
        friend std::string parseCircuitSeq(const std::string &inFilename,
                                        int version,
                                        const std::optional<std::string> &outFilename,
                                        const std::optional<float> &multiplier);

        friend std::string parseCircuit(const std::string &inFilename,
                                        bool useOpenMP,
                                        int version,
                                        const std::optional<std::string> &outFilename,
                                        const std::optional<float> &multiplier);
    };

    /**
     * Parse provided circuit ansatz in input .txt file into OpenQASM, store in string member `qasm` and optionally
     * write to file. Sequential function, no parallel execution.
     * @param inFilename Path to input file containing circuit in string representation.
     * @param version If set true, equals OpenQASM version 3; Version 2 for false (default)
     * @param outFilename Optional; If provided, write OpenQASM representation into this file. Must be .txt file!
     * @param multiplier Optional; Multiplier to multiply all operators with.
     * @return OpenQASM version of input file
     */
    std::string parseCircuitSeq(const std::string &inFilename,
                             int version = 2,
                             const std::optional<std::string> &outFilename = std::nullopt,
                             const std::optional<float> &multiplier = std::nullopt);

    /**
     * Overloaded function to use parallelism. If the useOpenMP parameter is set, parallel execution is enabled,
     * according to the value. If it is not set, fall back to sequential execution.
     * @param inFilename Path to input file containing ansatz circuit in string representation
     * @param useOpenMP True: Use OpenMP as parallel framework; False: Use execution policy as parallel framework
     * @param version True: Version 3; False: Version 2
     * @param outFilename Optional; If provided, write OpenQASM representation into this file.
     * @param multiplier Optional; Multiplier to multiply all operators with
     * @return OpenQASM version of input file
     */
    std::string parseCircuit(const std::string &inFilename,
                             bool useOpenMP,
                             int version = 2,
                             const std::optional<std::string> &outFilename = std::nullopt,
                             const std::optional<float> &multiplier = std::nullopt);

    int test(int a, int b);
}

#endif //QASM_PARSER_PARSER_H
