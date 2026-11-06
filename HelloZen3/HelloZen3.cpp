
#include <iostream>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#pragma comment(lib, "odbc32.lib")

// function to execute any query and display its results
bool runQuery(SQLHSTMT stmt, const char* title, SQLCHAR* sql) {
    // just a label for clarify
    std::cout << "\n" << title << "\n";

	// execute the query, report any errors
    if (!SQL_SUCCEEDED(SQLExecDirectA(stmt, sql, SQL_NTS))) {
        SQLCHAR state[6], message[256];
        SQLINTEGER code;
        SQLSMALLINT length;

        if (SQL_SUCCEEDED(SQLGetDiagRecA(SQL_HANDLE_STMT, stmt, 1,
            state, &code, message, sizeof(message), &length)))
            std::cerr << "SQL error (" << state << "): " << message << "\n";
        return false;
    }

    // how many columns are there?
    SQLSMALLINT columns;
    SQLNumResultCols(stmt, &columns);

    // display headings based on column names
    for (SQLUSMALLINT i = 1; i <= columns; i++) {
        SQLCHAR heading[64]{};
        SQLDescribeColA(stmt, i, heading, sizeof(heading),
            nullptr, nullptr, nullptr, nullptr, nullptr);
        std::cout << (i > 1 ? " | " : "") << heading;
    }
    std::cout << "\n";

    // Display all rows as text
    SQLRETURN result;
    while (SQL_SUCCEEDED(result = SQLFetch(stmt))) {
        for (SQLUSMALLINT i = 1; i <= columns; i++) {
            SQLCHAR value[256]{};
            SQLLEN length;

            SQLGetData(stmt, i, SQL_C_CHAR, value, sizeof(value), &length);
            std::cout << (i > 1 ? " | " : "")
                << (length == SQL_NULL_DATA ? "NULL" : (char*)value);
        }
        std::cout << "\n";
    }

    SQLCloseCursor(stmt);
    return result == SQL_NO_DATA;
}

int main() {
    SQLHENV env;
    SQLHDBC db;
    SQLHSTMT stmt;

    // connect to Zen
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &db);
    SQLCHAR connection[] = "Driver={Pervasive ODBC Interface};DBQ=DEMODATA;";
    if (!SQL_SUCCEEDED(SQLDriverConnectA(db, nullptr, connection,
        SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT))) {
        std::cerr << "Connection failed!\n";
        return 1;
    }
    SQLAllocHandle(SQL_HANDLE_STMT, db, &stmt);

    // 1. SELECT: Retrieve courses
    SQLCHAR query1[] =
        "SELECT TOP 5 Name, Description, Credit_Hours "
        "FROM Course "
        "ORDER BY Name";
    if (!runQuery(stmt, "1. SELECT", query1)) return 1;
    system("pause");

    // 2. WHERE: Filter by department
    SQLCHAR query2[] =
        "SELECT Name, Description "
        "FROM Course "
        "WHERE Dept_Name = 'Art' ORDER BY Name";
    if (!runQuery(stmt, "2. WHERE", query2)) return 1;
    system("pause");

    // 3. ORDER BY: Sort by credit hours
    SQLCHAR query3[] =
        "SELECT TOP 5 Name, Credit_Hours "
        "FROM Course "
        "ORDER BY Credit_Hours DESC, Name";
    if (!runQuery(stmt, "3. ORDER BY", query3)) return 1;
    system("pause");

    // 4. AGGREGATES: Summarize courses by department
    SQLCHAR query4[] =
        "SELECT Dept_Name, COUNT(*) AS Course_Count, "
        "AVG(Credit_Hours) AS Avg_Credits "
        "FROM Course "
        "GROUP BY Dept_Name ORDER BY Dept_Name";
    if (!runQuery(stmt, "4. GROUP BY / Aggregates", query4)) return 1;
    system("pause");

    // 5. JOIN: Combine courses with their departments
    SQLCHAR query5[] =
        "SELECT TOP 5 c.Name, c.Description, d.Name AS Department "
        "FROM Course c "
        "INNER JOIN Dept d ON c.Dept_Name = d.Name "
        "ORDER BY c.Name";
    if (!runQuery(stmt, "5. JOIN", query5)) return 1;
    system("pause");

    // CLEANUP
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(db);
    SQLFreeHandle(SQL_HANDLE_DBC, db);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
