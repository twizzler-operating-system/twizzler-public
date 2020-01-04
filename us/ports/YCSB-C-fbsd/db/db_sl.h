//
//  basic_db.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/17/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#pragma once

#include "core/db.h"

#include "core/properties.h"
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <sqlite3.h>

using std::cout;
using std::endl;

struct ctx {
	std::vector<ycsbc::DB::KVPair> *result;
};

struct ctx2 {
	std::vector<std::vector<ycsbc::DB::KVPair>> *result;
};

namespace ycsbc
{
class SQLiteDB : public DB
{
  public:
	sqlite3 *db;
	int ready = 0;
	void Init()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		cout << "A new thread begins working!" << endl;

		if(ready == 0) {
			sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
			system("rm /tmp/test.db");
			int rc = sqlite3_open("/tmp/test.db", &db);

			sqlite3_exec(db, "PRAGMA synchronous = FULL", NULL, NULL, NULL);
			sqlite3_exec(db, "PRAGMA read_uncommitted = FALSE", NULL, NULL, NULL);
			sqlite3_exec(db, "PRAGMA journal_mode = TRUNCATE", NULL, NULL, NULL);
			// sqlite3_exec(db, "PRAGMA mmap_size=268435456;", NULL, NULL, NULL);
			char *zmsg;
			// fprintf(stderr, "CREATING TABLE %d\n", rc);
			rc = sqlite3_exec(db,
			  "CREATE TABLE usertable (key STRING NOT NULL, field0 STRING, field1 STRING, field2 "
			  "STRING, field3 STRING, field4 STRING, field5 STRING, field6 STRING, field7 STRING, "
			  "field8 STRING, field9 STRING, PRIMARY KEY (key));",
			  NULL,
			  NULL,
			  &zmsg);
			if(rc)
				fprintf(stderr, "ERR: %s\n", zmsg);
			ready = 1;
		}
	}

	static int callback(void *_c, int count, char **argv, char **coln)
	{
		struct ctx *c = (struct ctx *)_c;
		for(int i = 0; i < count; i++) {
			c->result->push_back(std::make_pair(std::string(coln[i]), std::string(argv[i])));
		}
		return 0;
	}

	static int callback2(void *_c, int count, char **argv, char **coln)
	{
		struct ctx2 *c = (struct ctx2 *)_c;
		std::vector<ycsbc::DB::KVPair> v;
		for(int i = 0; i < count; i++) {
			v.push_back(std::make_pair(std::string(coln[i]), std::string(argv[i])));
		}
		c->result->push_back(v);
		return 0;
	}

	int Read(const std::string &table,
	  const std::string &key,
	  const std::vector<std::string> *fields,
	  std::vector<KVPair> &result)
	{
		// std::lock_guard<std::mutex> lock(mutex_);

		std::string f;
		if(fields) {
			f = "(";
			for(auto fn : *fields) {
				f += fn + ", ";
			}
			f += ")";
		} else {
			f = "*";
		}

		std::string sql = "SELECT " + f + " FROM ";
		sql += table;
		sql += " WHERE key=\"";
		sql += key;
		sql += "\";";

		// fprintf(stderr, ":: %s\n", sql.c_str());
		struct ctx c;
		c.result = &result;
		char *zerr = 0;
		int rc = sqlite3_exec(db, sql.c_str(), callback, &c, &zerr);
		if(rc) {
			fprintf(stderr, "rERR: %s\n", zerr);
		}

		/*
		    cout << "READ " << table << ' ' << key;
		    if (fields) {
		      cout << " [ ";
		      for (auto f : *fields) {
		        cout << f << ' ';
		      }
		      cout << ']' << endl;
		    } else {
		      cout  << " < all fields >" << endl;
		    }*/
		return 0;
	}

	int Scan(const std::string &table,
	  const std::string &key,
	  int len,
	  const std::vector<std::string> *fields,
	  std::vector<std::vector<KVPair>> &result)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		std::string f;
		if(fields) {
			f = "(";
			for(auto fn : *fields) {
				f += fn + ", ";
			}
			f += ")";
		} else {
			f = "*";
		}

		std::string sql = "SELECT " + f + " FROM ";
		sql += table;
		sql += " WHERE key>=\"";
		sql += key;
		sql += "\" LIMIT " + std::to_string(len) + ";";

		// fprintf(stderr, ":: %s\n", sql.c_str());
		struct ctx2 c;
		c.result = &result;
		char *zerr = 0;
		int rc = sqlite3_exec(db, sql.c_str(), callback2, &c, &zerr);
		if(rc) {
			fprintf(stderr, "sERR: %s\n", zerr);
		}

		/*
		cout << "SCAN " << table << ' ' << key << " " << len;
		if (fields) {
		  cout << " [ ";
		  for (auto f : *fields) {
		    cout << f << ' ';
		  }
		  cout << ']' << endl;
		} else {
		  cout  << " < all fields >" << endl;
		}*/
		return 0;
	}

	int Update(const std::string &table, const std::string &key, std::vector<KVPair> &values)
	{
		// std::lock_guard<std::mutex> lock(mutex_);

		std::string sql = "UPDATE " + table + " SET ";

		bool f = false;
		for(auto v : values) {
			if(f) {
				sql += ", ";
			}
			f = true;
			sql += v.first + "=\"" + v.second + "\"";
		}

		sql += " WHERE key=\"" + key + "\"";

		// fprintf(stderr, ":: %s\n", sql.c_str());
		char *zerr = 0;
		int rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &zerr);
		if(rc) {
			fprintf(stderr, "uERR: %s\n", zerr);
		}

		/*

		    cout << "UPDATE " << table << ' ' << key << " [ ";
		    for (auto v : values) {
		      cout << v.first << '=' << v.second << ' ';
		    }
		    cout << ']' << endl;*/
		return 0;
	}

	int Insert(const std::string &table, const std::string &key, std::vector<KVPair> &values)
	{
		// std::lock_guard<std::mutex> lock(mutex_);

		std::string sql = "INSERT INTO " + table + " ";

		std::string s1, s2;
		bool f = false;
		for(auto v : values) {
			if(f) {
				s1 += ", ";
				s2 += ", ";
			}
			f = true;
			s1 += v.first;
			s2 += "\"" + v.second + "\"";
		}

		sql += "(key, " + s1 + ") VALUES (\"" + key + "\", " + s2 + ");";

		// fprintf(stderr, ":: %s\n", sql.c_str());
		char *zerr = 0;
		int rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &zerr);
		if(rc) {
			fprintf(stderr, "iERR: %s\n", zerr);
		}

		/*

		    cout << "INSERT " << table << ' ' << key << " [ ";
		    for (auto v : values) {
		      cout << v.first << '=' << v.second << ' ';
		    }
		    cout << ']' << endl;*/
		return 0;
	}

	int Delete(const std::string &table, const std::string &key)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		cout << "DELETE " << table << ' ' << key << endl;
		return 0;
	}

  private:
	std::mutex mutex_;
};

} // ycsbc
