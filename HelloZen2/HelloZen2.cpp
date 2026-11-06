
#include <iostream>
#include <string>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#pragma comment(lib, "odbc32.lib")

// Represents a database row. ODBC maps its fields manually.
struct Customer {
    int id;
    std::string name;
    int shoeSize;
};

// function to execute SQL, check for errors
bool execute(SQLHSTMT stmt, SQLCHAR* sql) {
    SQLRETURN result = SQLExecDirectA(stmt, sql, SQL_NTS);

    if (!SQL_SUCCEEDED(result)) {
        SQLCHAR state[6], message[256];
        SQLINTEGER code;
        SQLSMALLINT length;

        if (SQL_SUCCEEDED(SQLGetDiagRecA(SQL_HANDLE_STMT, stmt, 1,
            state, &code, message, sizeof(message), &length)))
            std::cerr << "SQL error (" << state << "): " << message << "\n";
        else
            std::cerr << "SQL execution failed.\n";
    }

    return SQL_SUCCEEDED(result);
}

int main() {
    SQLHENV env;
    SQLHDBC db;
    SQLHSTMT stmt;

    // CONNECT TO ZEN
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &db);

    SQLCHAR connection[] = "Driver={Pervasive ODBC Interface};DBQ=DEMODATA;";
    // For databases requiring authentication:
    // SQLCHAR connection[] = "Driver={Pervasive ODBC Interface};DBQ=myDatabase;UID=myUser;PWD=myPassword;";

    if (!SQL_SUCCEEDED(SQLDriverConnectA(db, nullptr, connection,
        SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT))) {
        std::cerr << "Connection failed!\n";
        SQLFreeHandle(SQL_HANDLE_DBC, db);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 1;
    }

    SQLAllocHandle(SQL_HANDLE_STMT, db, &stmt);

    // SETUP: Create the table if it doesn't exist
    SQLCHAR setup[] = "CREATE TABLE IF NOT EXISTS TutorialCustomers "
        "(ID IDENTITY, Name VARCHAR(50), ShoeSize INTEGER, PRIMARY KEY (ID))";
    if (!execute(stmt, setup)) return 1;

    // CREATE: Insert a Customer object
    Customer customer{ 0, "Alice", 8 };
    SQLLEN textLength = SQL_NTS;

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
        SQL_VARCHAR, 50, 0, customer.name.data(),
        customer.name.size() + 1, &textLength);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG,
        SQL_INTEGER, 0, 0, &customer.shoeSize, 0, nullptr);

    SQLCHAR insert[] = "INSERT INTO TutorialCustomers (Name, ShoeSize) VALUES (?, ?)";
    if (!execute(stmt, insert)) return 1;
    SQLFreeStmt(stmt, SQL_RESET_PARAMS);

    // Get the automatically generated ID.
    SQLCHAR identity[] = "SELECT @@IDENTITY";
    if (!execute(stmt, identity)) return 1;
    SQLFetch(stmt);
    SQLGetData(stmt, 1, SQL_C_SLONG, &customer.id, sizeof(customer.id), nullptr);
    SQLCloseCursor(stmt);

    std::cout << "Inserted " << customer.name << " with ID " << customer.id << "\n";
    system("pause");

    // READ: Retrieve a Customer object by ID.
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
        SQL_INTEGER, 0, 0, &customer.id, 0, nullptr);

    SQLCHAR select[] = "SELECT ID, Name, ShoeSize FROM TutorialCustomers WHERE ID = ?";
    if (!execute(stmt, select)) return 1;

    Customer found{};
    char name[51]{};

    if (SQL_SUCCEEDED(SQLFetch(stmt))) {
        SQLGetData(stmt, 1, SQL_C_SLONG, &found.id, sizeof(found.id), nullptr);
        SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), nullptr);
        SQLGetData(stmt, 3, SQL_C_SLONG, &found.shoeSize, sizeof(found.shoeSize), nullptr);
        found.name = name;

        std::cout << "Found: " << found.id << " - " << found.name
            << ", Shoe Size: " << found.shoeSize << "\n";
        system("pause");
    }

    SQLCloseCursor(stmt);
    SQLFreeStmt(stmt, SQL_RESET_PARAMS);

    // UPDATE: Modify the Customer object and save changes.
    customer.name = "Bob";
    customer.shoeSize = 10;

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
        SQL_VARCHAR, 50, 0, customer.name.data(),
        customer.name.size() + 1, &textLength);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG,
        SQL_INTEGER, 0, 0, &customer.shoeSize, 0, nullptr);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG,
        SQL_INTEGER, 0, 0, &customer.id, 0, nullptr);

    SQLCHAR update[] = "UPDATE TutorialCustomers SET Name = ?, ShoeSize = ? WHERE ID = ?";
    if (!execute(stmt, update)) return 1;
    SQLFreeStmt(stmt, SQL_RESET_PARAMS);

    std::cout << "Updated customer " << customer.id << " to "
        << customer.name << ", Shoe Size: " << customer.shoeSize << "\n";
    system("pause");

    // DELETE: Remove the customer by ID.
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
        SQL_INTEGER, 0, 0, &customer.id, 0, nullptr);

    SQLCHAR remove[] = "DELETE FROM TutorialCustomers WHERE ID = ?";
    if (!execute(stmt, remove)) return 1;
    SQLFreeStmt(stmt, SQL_RESET_PARAMS);

    std::cout << "Deleted customer " << customer.id << "\n";
    system("pause");

    // CLEANUP
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(db);
    SQLFreeHandle(SQL_HANDLE_DBC, db);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}