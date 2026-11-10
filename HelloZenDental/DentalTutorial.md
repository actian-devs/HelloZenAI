# Build a Dental Billing Code Suggester with Actian Zen v17 AI and C++

*Add meaning-based search to an existing Zen application using vectors, an embedding model, and familiar SQL.*

A dental practice management application has plenty to do: scheduling, charting, insurance, payments, and more. In this tutorial, we'll focus on just one feature: helping a biller find likely dental procedure codes by comparing a doctor's note with previously billed treatments.

We'll use **Actian Zen v17 AI**, **C++/ODBC**, and a local embedding model through **Microsoft Foundry Local**. If you already know Zen and SQL but haven't worked with vectors, you're the intended audience.

> **Important:** All patient notes and insurance company names in this example are fictional. The program suggests historical matches, not medically verified billing codes. A qualified person must confirm the correct code.

## 1. The problem: different words, similar treatments

Imagine a biller receives this note:

> *"Pulled back gum, scaled below gumline on lower left."*

The biller needs to select the appropriate CDT procedure code. Experienced staff may recognize what's being described, but the answer can depend on details such as the number of teeth treated. A new employee may need to consult a coding reference, ask a colleague, or search past claims.

That creates a few familiar problems: tribal knowledge, inconsistent interpretations, additional review time, and claims that may need correction or follow-up with insurers.

Why not search the existing notes with `LIKE`?

```sql
SELECT * FROM DentalClaims
WHERE TreatmentNote LIKE '%root planing%';
```

That can find *"periodontal root planing,"* but it can miss *"deep cleaning below gums."* Conversely, matching the same keywords doesn't guarantee the procedures should receive the same code.

It would be useful if we could tell the computer, **"You know what I mean. Find earlier notes describing something similar,"** without requiring the same wording.

## 2. Vector search: finding what's nearby

Before involving AI, consider an ordinary map. A place can be represented by two numbers: latitude and longitude. For example, `40.758896, -73.985130` identifies a point around Times Square in New York City.

On a map, we can find nearby locations even when their names have nothing in common:

```text
                  Latitude (north)
                         ^
                         |
            Cafe A  o    |             o Cafe C
                         |
                 YOU *   |
                      o  | Cafe B
                         +--------------------> Longitude (east)
```

*Conceptual two-dimensional example, not a street map.*

We don't compare the spelling of location names. We compare positions. That's the essential idea behind **nearest-neighbor vector search**: give the system a point represented by numbers, and ask which stored points are closest.

In the map example, each point has **two dimensions**. Vector search can also work with hundreds or thousands of dimensions. The question becomes: where do those numbers come from, and why would similar dental notes end up near one another?

## 3. Embeddings: turning text into coordinates

An **embedding model** is trained on large amounts of text to learn useful relationships between words and passages. Once trained, it can take new text and return a list of numbers called an *embedding vector*.

For this tutorial, **Qwen3-Embedding-0.6B** produces **1,024 numbers** for each note. The first two might look something like this:

```text
[-0.0457, -0.0371, ...]  (1,024 dimensions)
```

Those individual numbers are not human-readable labels. Just as latitude and longitude mean something because they're part of a geographic coordinate system, the embedding numbers are meaningful **within the coordinate system learned by the model**.

Text describing related treatments will often produce nearby vectors. We can then use Zen's vector search to retrieve earlier notes that are similar **in meaning**, even when the words differ. This is commonly called **semantic search**.

Similarity isn't proof that a billing code is correct. The model may miss important clinical differences, and the historical claims might contain errors. Our goal is to help the biller find useful precedents more quickly, with a human making the final decision.

## 4. What we're building

The workflow is deliberately small:

```text
Doctor's note entered by biller
             |
             v
Qwen3 embedding model (local)
             |
             v
1,024-number query vector
             |
             v
Zen vector collection search
             |
             v
SQL JOIN to DentalClaims + optional insurer filter
             |
             v
Three most relevant historical claims
```

We won't insert or update claims in the application. The sample dataset already contains historical notes and their embeddings.

## 5. Prerequisites and sample database

You'll need:

- **Actian Zen v17 AI**, with its vector capabilities enabled, plus **Zen Control Center (ZenCC)**.
- **Visual Studio Community 2026**, with the *Desktop development with C++* workload.
- The **Pervasive ODBC Interface** driver configured for your Zen installation.
- The sample **HelloZenDental** C++ project, which uses the `Microsoft.AI.Foundry.Local.Runtime` NuGet package and C++20.

If you've completed the earlier HelloZen C++/ODBC tutorials, you can reuse that environment. **[Editorial note: insert the published link to the earlier setup tutorial and the GitHub repository here.]**

In ZenCC, create a new database named **`DENTAL`**. Its files can go in any suitable directory, such as `HelloZenDental/data` in your local checkout.

Next, open the sample SQL script in ZenCC's SQL Editor, select **`DENTAL`**, and run it. The repository's sample file may be named `HelloZenDental/dataset.sql` or `DentalSetup.sql`; use the filename actually included with the published project.

> **Heads-up:** Setup scripts can drop/recreate sample tables or create a vector collection. Use a fresh demo database, and don't run the script against real data. If you've already created the collection, follow the script's instructions for resetting it before importing again.

### Two kinds of data, connected by an ID

The ordinary relational table contains the billing details:

```sql
INSERT INTO DentalClaims
    (ClaimID, TreatmentNote, ProcedureCode, Insurer, FeeSchedule)
VALUES (
    4,
    'Routine adult dental cleaning and polishing',
    'D1110',
    'PearlyGate',
    'Preferred'
);
```

The vector collection contains the embedding of that note:

```sql
-- Illustrative excerpt: full vector omitted, so this is not executable SQL.
dbo.vector_point_upsert(
    'dental_claim_embeddings',
    '{"u64_id":4,"dim":1024}',
    '[-0.04565172,-0.0371384, ... 1022 more numbers ...]',
    '{"source":"DentalClaims","claim_id":4}'
);
```

The important connection is **`u64_id = 4`** in the vector collection matching **`ClaimID = 4`** in the relational table. We will JOIN on those IDs. The JSON payload also records the claim ID as optional metadata, but the JOIN does **not** depend on the payload.

This is the first useful thing to know about Zen vector collections: **they aren't ordinary relational tables**. You work with them through functions such as `dbo.vector_point_upsert()` and `dbo.vector_collection_search()`. They don't appear as regular DDF-backed tables in Zen Explorer.

See the [Zen v17 Vector Collection Functions reference](https://docs.actian.com/zen/v17/sqlref/vectorcollectionfuncs.htm).

## 6. Choose an embedding model

We're using **Qwen3-Embedding-0.6B**, accessed through **Microsoft Foundry Local**. This is an embedding model, not a chatbot. Its job here is to turn text into numerical vectors.

Why this combination? Foundry Local gives our C++ application a straightforward way to download, load, and use the model **inside the process**. There's no separate model service to start and no cloud API key needed.

That's a convenient choice for a tutorial, not the only architecture. Alternatives include Ollama or another model-hosting service accessed over HTTP, other local inference libraries, and hosted embedding APIs from cloud providers. You can also build or fine-tune models for particular needs, although that adds substantial work.

Local processing can mean lower ongoing inference costs and more control over where sensitive notes are processed. Once the model and runtime are installed and cached, inference can work without internet access, assuming Zen and the rest of the deployment are also local. It does **not**, by itself, establish regulatory compliance. For the larger discussion, see [Actian's Data & AI Sovereignty overview](https://www.actian.com/solutions/data-ai-sovereignty/).

**One critical rule:** The historical notes and new search text must be embedded using the **same compatible model and preprocessing approach**. You can't meaningfully compare a query from one model against vectors generated by an unrelated model. The sample data was generated using this Qwen model.

## 7. Walk through the C++ application

Open `HelloZenDental.cpp` in the provided Visual Studio solution. We'll concentrate on the portions that are new; the ODBC plumbing should be familiar from the earlier examples.

### Load the model

```cpp
Configuration config("ZenDentalDemo");
Manager manager(std::move(config));

auto model = manager.GetCatalog().GetModel("qwen3-embedding-0.6b");

if (!model->IsCached())
    model->Download();

model->Load();
```

On first use, Foundry Local downloads the model, so startup can take a while. Subsequent runs use the cached copy. Loading the model into memory still takes some time, which is why we do it **once at startup**, not for every search.

Then create an embedding session:

```cpp
EmbeddingsSession session(*model);
```

We reuse this session for each treatment note entered by the user.

### Connect to Zen over ODBC

```cpp
SQLCHAR connection[] =
    "Driver={Pervasive ODBC Interface};DBQ=DENTAL;";

SQLDriverConnectA(db, nullptr, connection, SQL_NTS,
    nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
```

The complete project also checks for connection errors and prints ODBC diagnostics. It's the same familiar ODBC interface, with a new kind of query coming next. Some Zen configurations also require a VDE session credential for vector operations; see the references below if your installation does.

### Accept a treatment note and optional insurer

The console asks for the doctor's note and, optionally, an insurer. Try:

```text
pulled back gum, scaled below gumline on lower left
```

Leave the insurer blank to search across all insurers, or enter an exact name such as `PearlyGate`.

The insurer is a normal **relational SQL filter**. It doesn't change how the text is embedded. In this sample, a blank insurer produces no `WHERE` clause, and a supplied name produces a condition on `c.Insurer`.

### Generate the query embedding

```cpp
std::vector<float> embedding = session.Embed(note);
std::string vectorJson = vectorToJsonArray(embedding);
```

The first line is where the AI model does its work. The second formats the C++ vector as the JSON numeric array expected by Zen, such as `[0.0123,-0.0456,...]`. `vectorToJsonArray()` also handles numeric formatting and rejects invalid values.

### Search vectors and JOIN to the claims table

This is the heart of the example:

```cpp
std::string query = std::format(R"SQL(
    SELECT TOP 3
        c.ClaimID,
        c.ProcedureCode,
        c.TreatmentNote,
        c.Insurer,
        c.FeeSchedule,
        v."SIMILARITY_SCORE"
    FROM dbo.vector_collection_search(
        'dental_claim_embeddings', NULL, '{}', 50, {}
    ) v
    INNER JOIN DentalClaims c
        ON CAST(v.U64_ID AS INTEGER) = c.ClaimID
    {}
    ORDER BY v."SIMILARITY_SCORE" DESC
)SQL", vectorJson, embedding.size(), filter);
```

A few things are happening:

- `vector_collection_search(...)` asks Zen for nearest-neighbor matches based on the query vector.
- `NULL` is a reserved argument in this function.
- `50` requests **up to 50 vector candidates** before the relational JOIN and insurer filter.
- `embedding.size()` supplies the vector dimension (1,024 for our model).
- `v.U64_ID = c.ClaimID` associates each vector match with the historical claim.
- The optional `WHERE` clause restricts matches to a particular insurer.
- `ORDER BY ... DESC` ranks the remaining rows by similarity, and `SELECT TOP 3` returns the top three.

**Why 50 candidates but only three results?** If we retrieved just three vectors and all belonged to other insurers, the filter could leave us with nothing. Retrieving a broader candidate set gives SQL more matches to work with. It's still a candidate limit, though, not a guarantee of finding the best three *within* a particular insurer; more selective searches may need a larger value or a different filtering strategy.

`std::format()` uses the `{}` placeholders to insert the JSON vector, its dimension, and the optional SQL filter. This keeps the query readable, but **it is not SQL parameterization**.

For a production application, **do not concatenate untrusted insurer text directly into SQL**. The small console demo does this for readability; a real application should use ODBC parameter binding (`SQLBindParameter`) and validate inputs. The notes themselves are sent to the embedding model, not inserted into this SQL string.

See the [official `dbo.vector_collection_search()` syntax and examples](https://docs.actian.com/zen/v17/sqlref/vectorcollectionfuncs.htm).

### Execute and display the matches

```cpp
SQLExecDirectA(stmt, (SQLCHAR*)query.data(), SQL_NTS);
```

The complete application checks the return value and reports ODBC errors. It then calls `SQLFetch()` and `SQLGetData()` to read each row and display the procedure code, historical note, insurer, fee schedule, and similarity score.

The score tells us which notes Zen considers more similar **for this model and dataset**. It is **not** a probability that the code is correct. A biller should inspect the full treatment details and confirm the appropriate code from current coding guidance.

## 8. Build and run

1. Restore the NuGet packages for the provided Visual Studio project, if prompted.
2. Select **Debug | x64** and set **HelloZenDental** as the startup project.
3. Confirm the `DENTAL` database has been created and populated.
4. Press **Ctrl+F5**. The first model download takes longer; later starts use the cache.
5. Enter a note, optionally filter by insurer, and review the three suggested historical claims.

Try wording that doesn't exactly match the sample notes:

- `Patient needed a deep cleaning under the gums on several lower-left teeth`
- `Removed an erupted tooth with forceps, no surgical sectioning`
- `Applied protective coating to the grooves of a back tooth`
- `Routine checkup to assess teeth and gums`

Try each with and without an insurer. If a filter produces no results, remember that Zen only sends the nearest 50 vector candidates into the subsequent relational filtering step.

## 9. What you've learned

Ordinary SQL remains valuable. An exact insurer name, procedure code, or fee schedule is still something to filter with a relational `WHERE` clause. **Vector search complements SQL; it doesn't replace it.**

The new idea is that we can use a model to turn unstructured notes into numbers representing relationships between descriptions. Zen can search those vectors for nearby matches, and a familiar SQL JOIN brings back the original records and business context.

That gives an existing application a useful new capability: **find similar records by meaning, then apply the relational rules it already understands.**

As you experiment, remember that similarity rankings are suggestions. Clinical coding decisions, payer rules, and data quality still require human judgment. This demonstration does not validate billing accuracy.

## 10. Next steps

Try more variations in wording, experiment with the candidate count, and observe how restricting the insurer changes the results. Examine the sample SQL to see how new historical notes are inserted and embedded. If your application later writes new claims, you'll need to generate an embedding with the same model and upsert a corresponding vector for each new record.

You can also extend the SQL filter to `FeeSchedule`, compare results against a traditional `LIKE` query, or try a different embedding model after regenerating **all** stored vectors with that model.

### References

- [Zen v17: Vector Collection Functions](https://docs.actian.com/zen/v17/sqlref/vectorcollectionfuncs.htm)
- [Actian: Build a Product Recommendation Engine in Pure SQL](https://www.actian.com/blog/developer/build-a-product-recommendation-engine-in-pure-sql/)
- [Zen v17 Developer Guide](https://docs.actian.com/zen/v17/prog_gde/About.htm)
- [Microsoft Foundry Local C++ SDK](https://github.com/microsoft/foundry-local/blob/main/sdk_v2/cpp/README.md)
- [Qwen3 Embedding models](https://github.com/QwenLM/Qwen3-Embedding)
- [Actian: Data & AI Sovereignty](https://www.actian.com/solutions/data-ai-sovereignty/)
