import duckdb

sf_all = duckdb.query("select * from 'snowset-main.parquet/part.1.parquet'").df()

sf_all