gcc -o BoaCom.cgi  ComWithHttpClient.c common.c Communicate.c namedtest.c otdr.c defaulttest.c cycletest.c responed.c sql.c process.c -lsqlite3 -lmxml -lpthread -D_GNU_SOURCE -D__USE_XOPEN

gcc -o cycMain  cycMain.c sql.c common.c uploadCycTestData.c otdr.c process.c  -lsqlite3 -lcurl -D_GNU_SOURCE -D__USE_XOPEN


