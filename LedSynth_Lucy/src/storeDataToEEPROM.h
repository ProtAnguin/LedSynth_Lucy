#include <EEPROM.h>

#define N_COLUMNS  D_NLS       // Change based on your data size
#define M_ROWS     N_ISOBANKS
#define TOTAL_VALUES (N_COLUMNS * M_ROWS)

// Peek at stored EEPROM values before deciding to load them
void peekFromEEPROM() {
    Serial.println("\nEEPROM Stored Data:");
    Serial.println("-------------------------------");

    for (int i = 0; i < M_ROWS; i++) {
        for (int j = 0; j < N_COLUMNS; j++) {
            int address = (i * N_COLUMNS + j) * sizeof(float);
            float value;
            EEPROM.get(address, value);
            Serial.print(value, 6);  // Print float with 6 decimal places
            Serial.print("\t");
        }
        Serial.println();
    }
    
    Serial.println("-------------------------------");
    Serial.println("Type 'Y' to load this data, or any other key to skip.");
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
    Serial.println("Data loaded successfully.");
}

// Save the current array to EEPROM
void saveToEEPROM() {
    Serial.println("Saving data to EEPROM...");
    for (int i = 0; i < M_ROWS; i++) {
        for (int j = 0; j < N_COLUMNS; j++) {
            int address = (i * N_COLUMNS + j) * sizeof(float);
            EEPROM.put(address, isoLog[i][j]);
        }
    }
    Serial.println("Save complete.");
}

// Set a particular value in the array
void setDataValue(int row, int col, float value) {
    if (row >= 0 && row < M_ROWS && col >= 0 && col < N_COLUMNS) {
        isoLog[row][col] = value;
        Serial.print("Set isoLog[");
        Serial.print(row);
        Serial.print("][");
        Serial.print(col);
        Serial.print("] = ");
        Serial.println(value, 6);
    } else {
        Serial.println("Error: Index out of bounds!");
    }
}
