
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <locale>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <foundry_local/foundry_local_cpp.h>

#pragma comment(lib, "odbc32.lib")

using namespace foundry_local;

// Convert an embedding into the JSON array Zen expects.
std::string vectorToJson(const std::vector<float>& embedding) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(7) << "[";

    for (size_t i = 0; i < embedding.size(); i++) {
        if (!std::isfinite(embedding[i]))
            throw std::runtime_error("Invalid embedding value.");

        if (i > 0) out << ",";
        out << embedding[i];
    }

    out << "]";

    if (out.str().size() > 14997)
        throw std::runtime_error("Vector exceeds Zen's SQL literal limit.");

    return out.str();
}

// Display an ODBC error, if one occurs.
void showError(SQLSMALLINT kind, SQLHANDLE handle) {
    SQLCHAR state[6]{}, message[512]{};
    SQLINTEGER code;
    SQLSMALLINT length;

    if (SQL_SUCCEEDED(SQLGetDiagRecA(kind, handle, 1,
        state, &code, message, sizeof(message), &length)))
        std::cerr << "ODBC error (" << state << "): " << message << "\n";
}

int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  ACTIAN ZEN AI - DENTAL CODE SUGGESTER\n";
        std::cout << "========================================\n\n";

        // LOAD THE EMBEDDING MODEL
        std::cout << "Loading Qwen3 embedding model...\n";

        Configuration config("ZenDentalDemo");
        Manager manager(std::move(config));

        auto model = manager.GetCatalog().GetModel("qwen3-embedding-0.6b");

        if (!model) {
            std::cerr << "Embedding model not found.\n";
            return 1;
        }

        if (!model->IsCached())
            model->Download();

        model->Load();

        {
            EmbeddingsSession session(*model);

            // CONNECT TO ZEN
            SQLHENV env;
            SQLHDBC db;
            SQLHSTMT stmt;

            SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
            SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
                (SQLPOINTER)SQL_OV_ODBC3, 0);
            SQLAllocHandle(SQL_HANDLE_DBC, env, &db);

            SQLCHAR connection[] =
                "Driver={Pervasive ODBC Interface};DBQ=DENTAL;";

            if (!SQL_SUCCEEDED(SQLDriverConnectA(db, nullptr,
                connection, SQL_NTS, nullptr, 0, nullptr,
                SQL_DRIVER_NOPROMPT))) {

                showError(SQL_HANDLE_DBC, db);
                SQLFreeHandle(SQL_HANDLE_DBC, db);
                SQLFreeHandle(SQL_HANDLE_ENV, env);
                return 1;
            }

            SQLAllocHandle(SQL_HANDLE_STMT, db, &stmt);

            std::cout << "Connected to Actian Zen (DENTAL).\n";
            std::cout << "Ready to search historical claims!\n";

            // INTERACTIVE SEARCH
            while (true) {
                std::string note;

                std::cout << "\n----------------------------------------\n";
                std::cout << "Enter a dental treatment note [Example: pulled back gum, scaled below gumline on lower left]\n";
                std::cout << "(blank to exit):\n> ";

                if (!std::getline(std::cin, note) || note.empty())
                    break;

                // Optional relational filter.
                std::string insurer;
                std::cout << "\nWhich insurer (leave blank for all)?\n";
                std::getline(std::cin, insurer);

                std::string filter;
                if (!insurer.empty()) {
                    filter = " WHERE c.Insurer = '" + insurer + "'";
                }

                // STEP 1: Generate an embedding.
                std::cout << "\n[1/3] Generating text embedding...\n";

                std::vector<float> embedding = session.Embed(note);

                std::cout << "      Embedding: ["
                    << std::fixed << std::setprecision(4)
                    << embedding[0] << ", " << embedding[1]
                    << ", ...] (" << embedding.size() << " dimensions)\n";

                // STEP 2: Search Zen's vector collection.
                std::cout << "[2/3] Searching similar historical claims...\n";

                std::string vectorJson = vectorToJson(embedding);

                // Find 50 vector candidates, then use SQL to join
                // claim details, filter, and return the top 3
                std::string query =
                    "SELECT TOP 3 "
                    "c.ClaimID, c.ProcedureCode, c.TreatmentNote, "
                    "c.Insurer, c.FeeSchedule, v.\"SIMILARITY_SCORE\" "
                    "FROM dbo.vector_collection_search("
                    "'dental_claim_embeddings', NULL, '" +
                    vectorJson + "', 50, " +
                    std::to_string(embedding.size()) + ") v "
                    "INNER JOIN DentalClaims c "
                    "ON CAST(v.U64_ID AS INTEGER) = c.ClaimID " +
                    filter +
                    " ORDER BY v.\"SIMILARITY_SCORE\" DESC";

                if (!SQL_SUCCEEDED(SQLExecDirectA(stmt,
                    (SQLCHAR*)query.data(), SQL_NTS))) {

                    showError(SQL_HANDLE_STMT, stmt);
                    SQLFreeStmt(stmt, SQL_CLOSE);
                    continue;
                }

                // STEP 3: Display matching claims.
                std::cout << "[3/3] Retrieving procedure codes...\n\n";
                std::cout << "SUGGESTED CODES FROM HISTORICAL CLAIMS\n";
                std::cout << "----------------------------------------\n";

                int matches = 0;

                while (SQL_SUCCEEDED(SQLFetch(stmt))) {
                    SQLINTEGER claimId;
                    SQLCHAR code[6]{}, treatment[256]{};
                    SQLCHAR claimInsurer[41]{}, feeSchedule[31]{};
                    SQLDOUBLE score;

                    SQLGetData(stmt, 1, SQL_C_SLONG,
                        &claimId, sizeof(claimId), nullptr);
                    SQLGetData(stmt, 2, SQL_C_CHAR,
                        code, sizeof(code), nullptr);
                    SQLGetData(stmt, 3, SQL_C_CHAR,
                        treatment, sizeof(treatment), nullptr);
                    SQLGetData(stmt, 4, SQL_C_CHAR,
                        claimInsurer, sizeof(claimInsurer), nullptr);
                    SQLGetData(stmt, 5, SQL_C_CHAR,
                        feeSchedule, sizeof(feeSchedule), nullptr);
                    SQLGetData(stmt, 6, SQL_C_DOUBLE,
                        &score, sizeof(score), nullptr);

                    matches++;

                    std::cout << matches << ". " << code
                        << "  |  Similarity: "
                        << std::fixed << std::setprecision(3)
                        << score << "\n";

                    std::cout << "   Historical note: "
                        << treatment << "\n";

                    std::cout << "   Claim #" << claimId
                        << " | " << claimInsurer
                        << " | " << feeSchedule << "\n\n";
                }

                if (matches == 0)
                    std::cout << "No matching claims found.\n";

                SQLCloseCursor(stmt);
            }

            // CLEANUP
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            SQLDisconnect(db);
            SQLFreeHandle(SQL_HANDLE_DBC, db);
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }

        model->Unload();

        std::cout << "\nGoodbye!\n";
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
