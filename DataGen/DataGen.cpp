
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <locale>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <foundry_local/foundry_local_cpp.h>

using namespace foundry_local;

// Fictional examples grouped by dental procedure code.
struct Procedure {
    std::string code;
    std::vector<std::string> notes;
};

// Escape apostrophes for SQL string literals.
std::string sqlString(const std::string& value) {
    std::string result = "'";
    for (char c : value)
        result += (c == '\'' ? "''" : std::string(1, c));
    return result + "'";
}

// Convert an embedding into a JSON array for Zen.
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
    std::string json = out.str();

    // Zen limits SQL string literals to about 15,000 bytes.
    if (json.size() > 14997)
        throw std::runtime_error("Vector exceeds Zen SQL literal limit.");

    return json;
}

int main(int argc, char* argv[]) {
    try {
        // Number of records to generate (default: 5).
        int count = argc > 1 ? std::stoi(argv[1]) : 5;

        if (count < 1 || count > 10000) {
            std::cerr << "Record count must be 1-10000.\n";
            return 1;
        }

        // Synthetic dental treatments.
        const std::vector<Procedure> procedures = {
            { "D4341",{
                "Scaling and root planing on five teeth in lower left quadrant",
                "Deep cleaning below gums on four teeth in upper right quadrant",
                "Removed tartar beneath gumline from six teeth, lower right",
                "Periodontal root planing for five teeth in upper left quadrant"
            } },
            { "D4342",{
                "Deep scaling performed on two teeth in lower left quadrant",
                "Root planing below gumline on one upper right molar",
                "Removed subgingival deposits from three teeth, upper left",
                "Periodontal scaling of two teeth in lower right quadrant"
            } },
            { "D7140",{
                "Simple extraction of erupted upper right molar",
                "Removed loose erupted tooth using forceps",
                "Routine extraction of erupted lower left premolar",
                "Extracted fully erupted tooth without surgical sectioning"
            } },
            { "D1110",{
                "Routine adult dental cleaning and polishing",
                "Preventive cleaning with plaque removal and polishing",
                "Adult prophylaxis completed with removal of light calculus",
                "Regular hygiene visit for scaling and polishing"
            } },
            { "D2391",{
                "Single surface composite filling on posterior tooth",
                "Repaired one chewing surface of molar using resin",
                "Placed tooth colored restoration on one posterior surface",
                "Removed decay and restored single surface with composite"
            } },
            { "D0274",{
                "Four bitewing dental X-rays obtained",
                "Captured four bitewing radiographs to check for decay",
                "Four bitewing images taken during dental examination",
                "Obtained four posterior bitewing radiographic views"
            } },
            { "D0120",{
                "Periodic oral examination for established patient",
                "Routine dental evaluation and oral health assessment",
                "Recall exam including evaluation of teeth and gums",
                "Periodic examination with updated dental findings"
            } }
        };

        const std::vector<std::string> insurers = {
            "Demo Dental A",
            "Demo Dental B",
            "Demo Dental C"
        };

        const std::vector<std::string> feeSchedules = {
            "Standard",
            "Preferred",
            "Community"
        };

        const std::vector<std::string> suffixes = {
            "",
            ". Patient tolerated treatment well",
            ". Post-treatment instructions reviewed",
            ". Visit completed without complications",
            ". Follow-up recommendations discussed"
        };

        // INITIALIZE FOUNDRY LOCAL
        std::cout << "Initializing Foundry Local...\n";

        Configuration config("ZenDentalDataGen");
        Manager manager(std::move(config));

        auto model = manager.GetCatalog()
            .GetModel("qwen3-embedding-0.6b");

        if (!model)
            throw std::runtime_error("Embedding model not found.");

        if (!model->IsCached()) {
            std::cout << "Downloading model...\n";
            model->Download();
        }

        std::cout << "Loading model...\n";
        model->Load();

        {
            EmbeddingsSession session(*model);

            // Open the SQL output file.
            std::ofstream sql("setup.sql");

            if (!sql)
                throw std::runtime_error("Cannot create setup.sql.");

            sql << "-- Actian Zen Dental Billing Tutorial\n";
            sql << "-- Fictional dental claims with real AI embeddings\n";
            sql << "-- Model: qwen3-embedding-0.6b\n";
            sql << "-- Run against the DentalTutorial database.\n\n";

            // SETUP: Create the relational table.
            sql << "DROP TABLE IF EXISTS DentalClaims;\n\n";

            sql << "CREATE TABLE DentalClaims (\n"
                "    ClaimID INTEGER PRIMARY KEY,\n"
                "    TreatmentNote VARCHAR(255),\n"
                "    ProcedureCode CHAR(5),\n"
                "    Insurer VARCHAR(40),\n"
                "    FeeSchedule VARCHAR(30)\n"
                ");\n\n";

            // Generate records and embeddings.
            int dimensions = 0;

            for (int i = 0; i < count; i++) {
                int claimId = i + 1;

                const Procedure& procedure =
                    procedures[i % procedures.size()];

                size_t variation =
                    (i / procedures.size()) % procedure.notes.size();

                std::string note = procedure.notes[variation];

                size_t suffix =
                    (i / (procedures.size() * 4)) % suffixes.size();

                note += suffixes[suffix];

                std::string insurer =
                    insurers[(i * 3 + i / 7) % insurers.size()];

                std::string feeSchedule =
                    feeSchedules[(i * 5 + i / 3) % feeSchedules.size()];

                // Generate a REAL embedding from the treatment note.
                std::vector<float> embedding = session.Embed(note);

                if (embedding.empty())
                    throw std::runtime_error("Model returned an empty vector.");

                // Create the vector collection once we know its dimensions.
                if (dimensions == 0) {
                    dimensions = static_cast<int>(embedding.size());

                    std::cout << "Embedding dimensions: "
                        << dimensions << "\n";

                    sql << "-- VECTOR COLLECTION\n";
                    sql << "-- If rerunning, drop this collection first:\n";
                    sql << "-- dbo.vector_collection_drop("
                        "'dental_claim_embeddings');\n\n";

                    sql << "dbo.vector_collection_create("
                        << "'dental_claim_embeddings', "
                        << "'{\"dimension\":" << dimensions
                        << ",\"distance_metric\":\"cosine\"}');\n\n";
                }

                if (static_cast<int>(embedding.size()) != dimensions)
                    throw std::runtime_error("Inconsistent vector dimensions.");

                // INSERT: Relational claim data.
                sql << "INSERT INTO DentalClaims "
                    "(ClaimID, TreatmentNote, ProcedureCode, "
                    "Insurer, FeeSchedule) VALUES (\n"
                    << "    " << claimId << ",\n"
                    << "    " << sqlString(note) << ",\n"
                    << "    " << sqlString(procedure.code) << ",\n"
                    << "    " << sqlString(insurer) << ",\n"
                    << "    " << sqlString(feeSchedule) << "\n"
                    << ");\n\n";

                // UPSERT: Matching vector, using ClaimID as its ID.
                sql << "dbo.vector_point_upsert(\n"
                    << "    'dental_claim_embeddings',\n"
                    << "    '{\"u64_id\":" << claimId
                    << ",\"dim\":" << dimensions << "}',\n"
                    << "    '" << vectorToJson(embedding) << "',\n"
                    << "    '{\"source\":\"DentalClaims\","
                    "\"claim_id\":" << claimId << "}'\n"
                    << ");\n\n";

                std::cout << "\rGenerated " << claimId
                    << " of " << count << std::flush;
            }

            sql << "\n-- Verify the imported claims.\n";
            sql << "SELECT * FROM DentalClaims;\n";

            sql.close();

            std::cout << "\n\nGenerated setup.sql successfully!\n";
            std::cout << "Records: " << count << "\n";
            std::cout << "Dimensions: " << dimensions << "\n";
            std::cout << "File: "
                << std::filesystem::absolute("setup.sql")
                << "\n";
        }

        model->Unload();
    }
    catch (const std::exception& error) {
        std::cerr << "\nError: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
