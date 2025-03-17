#include <EEPROM.h>

#define N_COLUMNS  D_NLS       // Change based on your data size
#define M_ROWS     N_ISOBANKS
#define TOTAL_VALUES (N_COLUMNS * M_ROWS)

// Peek at stored EEPROM values before deciding to load them
void peekFromEEPROM() {
    Serial.println("\nEEPROM Stored Data:");
    Serial.println("-------------------------------");

    for (int i = 0; i < M_ROWS; i++) {
        if(i == isoLogCurr) { Serial.print("X\t"); } else { Serial.print("\t"); }

        for (int j = 0; j < N_COLUMNS; j++) {
            int address = (i * N_COLUMNS + j) * sizeof(float);
            float value;
            EEPROM.get(address, value);
            Serial.print(value, 3);  // Print float with 6 decimal places
            Serial.print("\t");
        }
        Serial.println();
    }
}

// Load EEPROM data into the main array
void loadFromEEPROM() {
    Serial.println("Loading stored data...");
    for (int i = 0; i < M_ROWS; i++) {
        for (int j = 0; j < N_COLUMNS; j++) {
            int address = (i * N_COLUMNS + j) * sizeof(float);
            EEPROM.get(address, isoLog[i][j]);
        }
    }
    Serial.println("EEPROM data loaded successfully.");
}

// Save the current array to EEPROM
void saveToEEPROM() {
    Serial.println("Saving data to EEPROM...");
    for (int i = 0; i < M_ROWS; i++) {
        for (int j = 0; j < N_COLUMNS; j++) {
            int address = (i * N_COLUMNS + j) * sizeof(float);
            EEPROM.put(address, isoLog[i][j]);
            // Serial.printf(" Saved at address %6i  value %1.3f\n", address, isoLog[i][j]);
        }
    }
    peekFromEEPROM();
    Serial.println("EEPROM save completed.");
}

// DANGER: force EEPROM isoLog to all zeros
void resetEEPROM() {
    for (int i = 0; i < M_ROWS; ++i) {
        for (int j = 0; j < N_COLUMNS; ++j) {
            isoLog[i][j] = 0.0;
        }
    }
    saveToEEPROM();
    Serial.println("EEPROM has been reset!");
}

// Set a particular value in the array
void setDataValue(int row, int col, float value) {
    if (row >= 0 && row < M_ROWS && col >= 0 && col < N_COLUMNS) {
        isoLog[row][col] = value;

        Serial.printf(" isoLog at [%2d][%2d] SET to value %1.3f (this does not SAVE to EEPROM)\n", row, col, value);
    } else {
        Serial.println("Error: Index out of bounds!");
    }
}
