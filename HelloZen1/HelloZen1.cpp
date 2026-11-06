#include <iostream>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <string>
#pragma comment(lib, "odbc32.lib")

// table definition for the Course table in Zen's DEMODATA database:
/*
CREATE TABLE "Course" IN DICTIONARY USING 'Course.mkd' PAGESIZE=4096 (
    "Name" CHAR(7) NOT NULL CASE,
    "Description" CHAR(50) CASE,
    "Credit_Hours" USMALLINT,
    "Dept_Name" CHAR(20) NOT NULL CASE
);
*/

int main() {
    SQLHENV env;
    SQLHDBC db;
    SQLHSTMT stmt;

    // Initialize ODBC.
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &db);

    // Connect to Zen's DEMODATA database (no DSN required).
    SQLCHAR connection[] = "Driver={Pervasive ODBC Interface};DBQ=DEMODATA;";
    // DEMODATA does not require credentials, but for databases that do:
    // SQLCHAR connection[] = "Driver={Pervasive ODBC Interface};DBQ=myDatabaseName;UID=myUser;PWD=myPassword;";

    if (!SQL_SUCCEEDED(SQLDriverConnectA(db, nullptr, connection,
        SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT))) {
        std::cerr << "Connection failed!\n";
        SQLFreeHandle(SQL_HANDLE_DBC, db);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 1;
    }

    // Query the Course table
    SQLAllocHandle(SQL_HANDLE_STMT, db, &stmt);
    SQLCHAR query[] = "SELECT Name, Description, Credit_Hours FROM Course";

    SQLRETURN result = SQLExecDirectA(stmt, query, SQL_NTS);

	// Check for errors
    if (!SQL_SUCCEEDED(result)) {
        // Display the SQL error returned by ODBC.
        SQLCHAR state[6], message[256];
        SQLINTEGER code;
        SQLSMALLINT length;

        if (SQL_SUCCEEDED(SQLGetDiagRecA(SQL_HANDLE_STMT, stmt, 1,
            state, &code, message, sizeof(message), &length)))
            std::cerr << "Query failed: " << message << "\n";
    } else {
        SQLCHAR name[8], description[51];
        SQLUSMALLINT creditHours;
        SQLLEN descLength, creditLength;

        // Read and display each row
        while (SQL_SUCCEEDED(SQLFetch(stmt))) {
            SQLGetData(stmt, 1, SQL_C_CHAR, name, sizeof(name), nullptr);
            SQLGetData(stmt, 2, SQL_C_CHAR, description, sizeof(description), &descLength);
            SQLGetData(stmt, 3, SQL_C_USHORT, &creditHours, sizeof(creditHours), &creditLength);

            if (descLength == SQL_NULL_DATA)
                description[0] = '\0';

            std::cout << name << ": " << description;

            // credit hours could be null, show 'unavailable' if so
            std::cout << (creditLength == SQL_NULL_DATA 
                ? " (credits unavailable)\n"
                : " (" + std::to_string(creditHours) + " credits)\n");
        }
    }

    // Clean up.
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(db);
    SQLFreeHandle(SQL_HANDLE_DBC, db);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}
