#include "config.h"

float readWeight() {
    static String buffer = "";

    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') { // Akhir data dari timbangan
            String numericPart = extractNumericPart(buffer);
            buffer = "";  // Reset setelah membaca
            if (numericPart.length() > 0) {
                return numericPart.toFloat();
            } else {
                return NAN;  // Data tidak valid
            }
        } 
        else if (buffer.length() < 20) { // Hindari buffer overflow
            buffer += c;
        } 
        else {
            buffer = ""; // Reset jika terlalu panjang
        }
    }
    return lastWeight;  // Gunakan berat terakhir jika tidak ada data baru
}
