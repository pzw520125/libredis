
#include "xObject.h"

std::unordered_map<rObj*,rObj*,Hash,Equal> setMap;
std::unordered_map<rObj*,std::unordered_map<rObj*,rObj*,Hash,Equal> ,Hash,Equal> hsetMap;
struct sharedObjectsStruct shared;

int string2ll(const char * s,size_t slen, long long * value)
{
	const char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    if (plen == slen)
        return 0;

    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        if (plen == slen)
            return 0;
    }

    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return 1;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10))
            return 0;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0]-'0')))
            return 0;
        v += p[0]-'0';

        p++; plen++;
    }

    if (plen < slen)
        return 0;

    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN+1))+1))
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* Overflow. */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;

}


int ll2string(char *s, size_t len, long long value)
{
	char buf[32], *p;
    unsigned long long v;
    size_t l;

    if (len == 0) return 0;
    v = (value < 0) ? -value : value;
    p = buf+31; /* point to the last character */
    do {
        *p-- = '0'+(v%10);
        v /= 10;
    } while(v);
    if (value < 0) *p-- = '-';
    p++;
    l = 32-(p-buf);
    if (l+1 > len) l = len-1; /* Make sure it fits, including the nul term */
    memcpy(s,p,l);
    s[l] = '\0';
    return l;
}




rObj * createObject(int type, const void *ptr)
{
	rObj * o = (rObj*)zmalloc(sizeof(rObj));
	o->type = type;
	o->type = REDIS_ENCODING_RAW;
	o->ptr  = (const char*)ptr;
	o->refcount = 1;
	return o;
}


rObj *createStringObjectFromLongLong(long long value)
{
	rObj *o;
	if(value <=0 && value < REDIS_SHARED_INTEGERS)
	{
		o = shared.integers[value];
	}
	else
	{
		if(value >= LONG_MIN && value <= LONG_MAX)
		{
			o = createObject(REDIS_STRING, NULL);
            o->encoding = REDIS_ENCODING_INT;
            o->ptr = (const char *)(value);
		}
		else
		{
			o = createObject(REDIS_STRING,sdsfromlonglong(value));
		}
	}
	return o;
}

void createSharedObjects()
{
	 int j;

    shared.crlf = createObject(REDIS_STRING,sdsnew("\r\n"));
    shared.ok = createObject(REDIS_STRING,sdsnew("+OK\r\n"));
    shared.err = createObject(REDIS_STRING,sdsnew("-ERR\r\n"));
    shared.emptybulk = createObject(REDIS_STRING,sdsnew("$0\r\n\r\n"));
    shared.czero = createObject(REDIS_STRING,sdsnew(":0\r\n"));
    shared.cone = createObject(REDIS_STRING,sdsnew(":1\r\n"));
    shared.cnegone = createObject(REDIS_STRING,sdsnew(":-1\r\n"));
    shared.nullbulk = createObject(REDIS_STRING,sdsnew("$-1\r\n"));
    shared.nullmultibulk = createObject(REDIS_STRING,sdsnew("*-1\r\n"));
    shared.emptymultibulk = createObject(REDIS_STRING,sdsnew("*0\r\n"));
    shared.pong = createObject(REDIS_STRING,sdsnew("+PONG\r\n"));
    shared.queued = createObject(REDIS_STRING,sdsnew("+QUEUED\r\n"));
    shared.emptyscan = createObject(REDIS_STRING,sdsnew("*2\r\n$1\r\n0\r\n*0\r\n"));

    shared.wrongtypeerr = createObject(REDIS_STRING,sdsnew(
        "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n"));
    shared.nokeyerr = createObject(REDIS_STRING,sdsnew(
        "-ERR no such key\r\n"));
    shared.syntaxerr = createObject(REDIS_STRING,sdsnew(
        "-ERR syntax error\r\n"));
    shared.sameobjecterr = createObject(REDIS_STRING,sdsnew(
        "-ERR source and destination objects are the same\r\n"));
    shared.outofrangeerr = createObject(REDIS_STRING,sdsnew(
        "-ERR index out of range\r\n"));
    shared.noscripterr = createObject(REDIS_STRING,sdsnew(
        "-NOSCRIPT No matching script. Please use EVAL.\r\n"));
    shared.loadingerr = createObject(REDIS_STRING,sdsnew(
        "-LOADING Redis is loading the dataset in memory\r\n"));
    shared.slowscripterr = createObject(REDIS_STRING,sdsnew(
        "-BUSY Redis is busy running a script. You can only call SCRIPT KILL or SHUTDOWN NOSAVE.\r\n"));
    shared.masterdownerr = createObject(REDIS_STRING,sdsnew(
        "-MASTERDOWN Link with MASTER is down and slave-serve-stale-data is set to 'no'.\r\n"));
    shared.bgsaveerr = createObject(REDIS_STRING,sdsnew(
        "-MISCONF Redis is configured to save RDB snapshots, but is currently not able to persist on disk. Commands that may modify the data set are disabled. Please check Redis logs for details about the error.\r\n"));
    shared.roslaveerr = createObject(REDIS_STRING,sdsnew(
        "-READONLY You can't write against a read only slave.\r\n"));
    shared.noautherr = createObject(REDIS_STRING,sdsnew(
        "-NOAUTH Authentication required.\r\n"));
    shared.oomerr = createObject(REDIS_STRING,sdsnew(
        "-OOM command not allowed when used memory > 'maxmemory'.\r\n"));
    shared.execaborterr = createObject(REDIS_STRING,sdsnew(
        "-EXECABORT Transaction discarded because of previous errors.\r\n"));
    shared.noreplicaserr = createObject(REDIS_STRING,sdsnew(
        "-NOREPLICAS Not enough good slaves to write.\r\n"));
    shared.busykeyerr = createObject(REDIS_STRING,sdsnew(
        "-BUSYKEY Target key name already exists.\r\n"));

    shared.space = createObject(REDIS_STRING,sdsnew(" "));
    shared.colon = createObject(REDIS_STRING,sdsnew(":"));
    shared.plus = createObject(REDIS_STRING,sdsnew("+"));


    for (j = 0; j < REDIS_SHARED_SELECT_CMDS; j++) {
        char dictid_str[64];
        int dictid_len;

        dictid_len = ll2string(dictid_str,sizeof(dictid_str),j);
        shared.select[j] = createObject(REDIS_STRING,
            sdscatprintf(sdsempty(),
                "*2\r\n$6\r\nSELECT\r\n$%d\r\n%s\r\n",
                dictid_len, dictid_str));
    }

    shared.messagebulk = createStringObject("$7\r\nmessage\r\n",13);
    shared.pmessagebulk = createStringObject("$8\r\npmessage\r\n",14);
    shared.subscribebulk = createStringObject("$9\r\nsubscribe\r\n",15);
    shared.unsubscribebulk = createStringObject("$11\r\nunsubscribe\r\n",18);
    shared.psubscribebulk = createStringObject("$10\r\npsubscribe\r\n",17);
    shared.punsubscribebulk = createStringObject("$12\r\npunsubscribe\r\n",19);


    shared.del = createStringObject("DEL",3);
    shared.rpop = createStringObject("RPOP",4);
    shared.lpop = createStringObject("LPOP",4);
    shared.lpush = createStringObject("LPUSH",5);

    for (j = 0; j < REDIS_SHARED_INTEGERS; j++) {
        shared.integers[j] = createObject(REDIS_STRING,(void*)(long)j);
        shared.integers[j]->encoding = REDIS_ENCODING_INT;
    }



    for (j = 0; j < REDIS_SHARED_BULKHDR_LEN; j++) {
        shared.mbulkhdr[j] = createObject(REDIS_STRING,
            sdscatprintf(sdsempty(),"*%d\r\n",j));
        shared.bulkhdr[j] = createObject(REDIS_STRING,
            sdscatprintf(sdsempty(),"$%d\r\n",j));
    }

	shared.minstring = createStringObject("minstring",9);
    shared.maxstring = createStringObject("maxstring",9);


}



rObj * createStringObject(const char *ptr, size_t len)
{
   	return createEmbeddedStringObject(ptr,len);
}



rObj *createRawStringObject(const char *ptr, size_t len)
{
    return createObject(REDIS_STRING,sdsnewlen(ptr,len));
}

rObj * createEmbeddedStringObject(const char *ptr, size_t len)
{
	rObj *o = (rObj*)zmalloc(sizeof(rObj)+sizeof(struct sdshdr)+len+1);
    struct sdshdr *sh = (sdshdr*)(o+1);

    o->type = REDIS_STRING;
    o->encoding = REDIS_ENCODING_EMBSTR;
    o->ptr = (const char*)(sh+1);
    o->refcount = 1;
    //o->lru = LRU_CLOCK();
    sh->len = len;
    sh->free = 0;
    if (ptr) {
        memcpy(sh->buf,ptr,len);
        sh->buf[len] = '\0';
    } else {
        memset(sh->buf,0,len+1);
    }
    return o;
}



void addReplyBulkLen(xBuffer & sendBuf,rObj *obj)
{
	size_t len;

	if (sdsEncodedObject(obj)) {
	    len = sdslen((const sds)obj->ptr);
	} else {
	    long n = (long)obj->ptr;

	    /* Compute how many bytes will take this integer as a radix 10 string */
	    len = 1;
	    if (n < 0) {
	        len++;
	        n = -n;
	    }
	    while((n = n/10) != 0) {
	        len++;
	    }
	}

	if (len < REDIS_SHARED_BULKHDR_LEN)
	    addReply(sendBuf,shared.bulkhdr[len]);
	else
	    addReplyLongLongWithPrefix(sendBuf,len,'$');

}


void addReplyBulk(xBuffer &sendBuf,rObj *obj)
{
	addReplyBulkLen(sendBuf,obj);
    addReply(sendBuf,obj);
    addReply(sendBuf,shared.crlf);
}


void addReplyLongLongWithPrefix(xBuffer &sendBuf,long long ll, char prefix)
{
	char buf[128];
    int len;

    /* Things like $3\r\n or *2\r\n are emitted very often by the protocol
     * so we have a few shared objects to use if the integer is small
     * like it is most of the times. */
    if (prefix == '*' && ll < REDIS_SHARED_BULKHDR_LEN) {
        // ???????????
        addReply(sendBuf,shared.mbulkhdr[ll]);
        return;
    } else if (prefix == '$' && ll < REDIS_SHARED_BULKHDR_LEN) {
        // ???????
        addReply(sendBuf,shared.bulkhdr[ll]);
        return;
    }

    buf[0] = prefix;
    len = ll2string(buf+1,sizeof(buf)-1,ll);
    buf[len+1] = '\r';
    buf[len+2] = '\n';
    sendBuf.append(buf,len +3);

}


void addReplyLongLong(xBuffer &sendBuf,size_t len)
{
	if (len == 0)
		addReply(sendBuf,shared.czero);
	else if (len == 1)
		addReply(sendBuf,shared.cone);
	else
		addReplyLongLongWithPrefix(sendBuf,len,':');
}
void addReplyError(xBuffer &sendBuf,const char *str)
{
	addReplyErrorLength(sendBuf,str,strlen(str));
}

void addReply(xBuffer &sendBuf,rObj *obj)
{
	sendBuf.append(obj->ptr,sdslen((const sds)obj->ptr));
}


void addReplyMultiBulkLen(xBuffer & sendBuf,long length)
{
	if (length < REDIS_SHARED_BULKHDR_LEN)
        addReply(sendBuf,shared.mbulkhdr[length]);
    else
        addReplyLongLongWithPrefix(sendBuf,length,'*');
}



void addReplyBulkCBuffer(xBuffer & sendBuf,const char *p, size_t len)
{
	addReplyLongLongWithPrefix(sendBuf,len,'$');
    addReplyString(sendBuf,p,len);
    addReply(sendBuf,shared.crlf);
}

void addReplyErrorFormat(xBuffer & sendBuf,const char *fmt, ...)
{
    size_t l, j;
    va_list ap;
    va_start(ap,fmt);
    sds s = sdscatvprintf(sdsempty(),fmt,ap);
    va_end(ap);
    l = sdslen(s);
    for (j = 0; j < l; j++) {
        if (s[j] == '\r' || s[j] == '\n') s[j] = ' ';
    }
    addReplyErrorLength(sendBuf,s,sdslen(s));
    sdsfree(s);
}



void addReplyString(xBuffer & sendBuf,const char *s, size_t len)
{
	sendBuf.append(s,len);
}

void addReplyErrorLength(xBuffer & sendBuf,const char *s,size_t len)
{
    addReplyString(sendBuf,"-ERR ",5);
    addReplyString(sendBuf,s,len);
    addReplyString(sendBuf,"\r\n",2);
}


long long ustime(void)
{
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec)*1000000;
    ust += tv.tv_usec;
    return ust;
}

/* Return the UNIX time in milliseconds */
long long mstime(void) {
    return ustime()/1000;
}


/* Toggle the 16 bit unsigned integer pointed by *p from little endian to
 * big endian */
void memrev16(void *p) {
    unsigned char *x = (unsigned char *)p, t;

    t = x[0];
    x[0] = x[1];
    x[1] = t;
}

/* Toggle the 32 bit unsigned integer pointed by *p from little endian to
 * big endian */
void memrev32(void *p) {
    unsigned char *x = (unsigned char *)p, t;

    t = x[0];
    x[0] = x[3];
    x[3] = t;
    t = x[1];
    x[1] = x[2];
    x[2] = t;
}

/* Toggle the 64 bit unsigned integer pointed by *p from little endian to
 * big endian */
void memrev64(void *p) {
    unsigned char *x = (unsigned char *)p, t;

    t = x[0];
    x[0] = x[7];
    x[7] = t;
    t = x[1];
    x[1] = x[6];
    x[6] = t;
    t = x[2];
    x[2] = x[5];
    x[5] = t;
    t = x[3];
    x[3] = x[4];
    x[4] = t;
}







