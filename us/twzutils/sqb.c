#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000ul;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	// if(NotUsed)
	//	return 0;
	// return 0;
	int i;
	for(i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
	return 0;
}

int trace_callback(unsigned trace, void *C, void *P, void *X)
{
	// if(trace == SQLITE_TRACE_PROFILE)
	//	printf("trace: %ld ns (%ld us)\n", *(uint64_t *)X, *(uint64_t *)X / 1000);
	return 0;
}

struct timespec start, end;
void start_timer(void)
{
	clock_gettime(CLOCK_MONOTONIC, &start);
}

uint64_t next_timer(void)
{
	clock_gettime(CLOCK_MONOTONIC, &end);
	struct timespec result;
	timespec_diff(&start, &end, &result);
	uint64_t r = result.tv_nsec + result.tv_sec * 1000000000ul;
	clock_gettime(CLOCK_MONOTONIC, &start);
	return r;
}

#define SQLE(name)                                                                                 \
	({                                                                                             \
		if(rc != SQLITE_OK) {                                                                      \
			fprintf(stderr, "SQL err %s: %d %s\n", name, rc, zErrMsg);                             \
			return 1;                                                                              \
		};                                                                                         \
	})

float random_float(int min, int max)
{
	return (float)(rand() % (max - min) + min);
}

int random_int(int min, int max)
{
	return rand() % (max - min) + min;
}

void random_name(char *name, size_t len)
{
	memset(name, 0, len);
	for(int i = 0; i < random_int(0, len - 8); i++) {
		*name++ = random_int('a', 'z');
	}
}

int main(int argc, char *argv[])
{
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	char *sql;
	sqlite3_stmt *res;

	/* Open database */
	printf("Opening\n");
	sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
#ifndef __twizzler__
	system("rm /tmp/test.db");
#endif
	rc = sqlite3_open("/tmp/test.db", &db);

	if(rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		return (0);
	}

	// sqlite3_trace_v2(db, SQLITE_TRACE_PROFILE, trace_callback, NULL);

	sql = "CREATE TABLE people ("
	      "FirstName          STRING    NOT NULL,"
	      "LastName           STRING    NOT NULL,"
	      "Salary             DECIMAL   NOT NULL,"
	      "Age                INT       NOT NULL,"
	      "Children           INT       NOT NULL,"
	      "Job                TEXT      NOT NULL);";

	/* Execute SQL statement */
	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

	SQLE("create");

#if 0
	rc = sqlite3_exec(db, "PRAGMA synchronous = OFF", NULL, NULL, &zErrMsg);
	SQLE("pragma sync");
	rc = sqlite3_exec(db, "PRAGMA read_uncommitted = TRUE", NULL, NULL, &zErrMsg);
	SQLE("pragma read_u");
	rc = sqlite3_exec(db, "PRAGMA mmap_size=2684354560;", NULL, NULL, &zErrMsg);
	SQLE("pragma mmap");

	rc = sqlite3_exec(db, "PRAGMA max_page_count=200000", NULL, NULL, &zErrMsg);
	SQLE("pragma mmap");
	//	rc = sqlite3_exec(db, "PRAGMA journal_mode = OFF", NULL, NULL, &zErrMsg);
	//	SQLE("pragma journal");
#endif
	sql = "INSERT INTO people VALUES (?, ?, ?, ?, ?, ?);";

	printf("Inserting\n");
	sqlite3_prepare_v2(db, sql, -1, &res, NULL);

#ifndef __twizzler__
	rc = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &zErrMsg);
#endif

	SQLE("begin");

	start_timer();
	int max = 100;
	for(int i = 0; i < max; i++) {
		if(i % (max / 10) == 0)
			printf("%d\n", i / (max / 10));
		char name[32];
		if(i == max / 2)
			strcpy(name, "abcde");
		else
			random_name(name, 32);
		rc = sqlite3_bind_text(res, 1, name, -1, SQLITE_TRANSIENT);
		SQLE("bind1");
		random_name(name, 32);
		rc = sqlite3_bind_text(res, 2, name, -1, SQLITE_TRANSIENT);
		SQLE("bind2");

		rc = sqlite3_bind_double(res, 3, random_float(0, 1000000));
		SQLE("bind3");

		rc = sqlite3_bind_int(res, 4, random_int(0, 100));
		SQLE("bind4");

		rc = sqlite3_bind_int(res, 5, random_int(0, 8));
		SQLE("bind5");

		random_name(name, 32);
		rc = sqlite3_bind_text(res, 6, name, -1, SQLITE_TRANSIENT);
		SQLE("bind6");

		rc = sqlite3_step(res);
		sqlite3_clear_bindings(res);
		rc = sqlite3_reset(res);
		SQLE("step");
	}
	uint64_t ns_insert = next_timer();

	printf("Ending\n");
#ifndef TWZ
	// rc = sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &zErrMsg);
#endif
	uint64_t ns_end = next_timer();
	SQLE("end");

	long quiet = 0;
	char *query;
	char *name;
	int q;
	q = atoi(argv[1]);
	switch(q) {
		case 0:
			query = "SELECT * FROM people;";
			quiet = 1;
			name = "Show-All-Rows";
			break;
		case 1:
			query = "SELECT COUNT(*) FROM people;";
			quiet = 0;
			name = "Count-Rows";
			break;
		case 2:
			query = "SELECT AVG(Salary) FROM people;";
			quiet = 0;
			name = "Calc-Mean";
			break;
		case 3:
			query = "SELECT AVG(Salary) FROM "
			        "(SELECT Salary FROM people ORDER BY Salary LIMIT 2 OFFSET "
			        "(SELECT (COUNT(*) - 1) / 2 FROM people));";
			// query = "SELECT Salary FROM people ORDER BY Salary LIMIT 8;";
			quiet = 0;
			name = "Calc-Median";
			break;
		case 4:
			query = "SELECT * FROM people ORDER BY Age;";
			quiet = 0;
			name = "Show-All-Rows-Ordered";
			break;
		case 5:
			query = "SELECT * FROM people WHERE FirstName = \"abcde\";";
			quiet = 0;
			name = "Needle-In-Haystack";
			break;
		case 6:

			printf("Building index\n");
			next_timer();
			rc = sqlite3_exec(
			  db, "CREATE INDEX idx ON people (FirstName);", callback, (void *)quiet, &zErrMsg);
			uint64_t ns_idx = next_timer();
			SQLE("exec idx");
			printf("ELAPSED (cgt) Create-Index %ld: %lf ms\n", max, ns_idx / 1000000.f);
			query = "SELECT * FROM people WHERE FirstName = \"abcde\";";
			quiet = 0;
			name = "Needle-In-Haystack-Index";
			break;
	}

	printf("Selecting\n");
	next_timer();
	rc = sqlite3_exec(db, query, callback, (void *)quiet, &zErrMsg);
	uint64_t ns_sel = next_timer();

	SQLE("sel");

	printf("Done!\n");
	sqlite3_close(db);

	printf("ELAPSED (cgt) %s %ld: ins %ld ; end %ld ; sel %ld ns\n",
	  name,
	  max,
	  ns_insert,
	  ns_end,
	  ns_sel);
	printf("ELAPSED (cgt) %s %ld: ins %lf ; end %lf ; sel %lf ms\n",
	  name,
	  max,
	  ns_insert / 1000000.f,
	  ns_end / 1000000.f,
	  ns_sel / 1000000.f);

#ifndef TWZ
	system("rm /tmp/test.db");
#endif

	return 0;
}
