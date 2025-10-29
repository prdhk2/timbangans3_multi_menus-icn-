#include "config.h"

String extractNumericPart(String inputString) {
    String numericPart = "";
    bool foundNumeric = false;
    bool foundDecimal = false;
    bool foundNegative = false;

    inputString.trim();

    for (int i = 0; i < inputString.length(); i++) {
        char currentChar = inputString.charAt(i);

        if (isdigit(currentChar)) {
            numericPart += currentChar;
            foundNumeric = true;
        } else if (currentChar == '.' && !foundDecimal && foundNumeric) {
            numericPart += currentChar;
            foundDecimal = true;
        } else if (currentChar == '-' && !foundNumeric && !foundNegative) {
            numericPart += currentChar;
            foundNegative = true;
        } else if (foundNumeric) {
            break;
        }
    }

    return numericPart;
}