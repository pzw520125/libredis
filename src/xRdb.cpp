#include "xRdb.h"
xRdb::xRdb()
{
	
}

xRdb::~xRdb()
{
	
}


int xRdb::rdbSaveLongLongAsStringObject(xRio *rdb, long long value)
{
	return 1;
}


int xRdb::rdbEncodeInteger(long long value, unsigned char *enc)
{
	if (value >= -(1<<7) && value <= (1<<7)-1) {
        enc[0] = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_INT8;
        enc[1] = value&0xFF;
        return 2;
    } else if (value >= -(1<<15) && value <= (1<<15)-1) {
        enc[0] = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_INT16;
        enc[1] = value&0xFF;
        enc[2] = (value>>8)&0xFF;
        return 3;
    } else if (value >= -((long long)1<<31) && value <= ((long long)1<<31)-1) {
        enc[0] = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_INT32;
        enc[1] = value&0xFF;
        enc[2] = (value>>8)&0xFF;
        enc[3] = (value>>16)&0xFF;
        enc[4] = (value>>24)&0xFF;
        return 5;
    } else {
        return 0;
    }
}

int xRdb::rdbTryIntegerEncoding(char *s, size_t len, unsigned char *enc)
{
	long long value;
	char *endptr,buf[32];

	value = strtoll(s,&endptr,10);
	if(endptr[0] != '\0')
	{
		return 0;
	}
	
	lll2string(buf,32,value);
	if (strlen(buf) != len || memcmp(buf,s,len)) 
	{
		return 0;	
	}

    return rdbEncodeInteger(value,enc);
	
}

void xRdb::startLoading(FILE *fp)
{
	struct stat sb;
	if(fstat(fileno(fp),&sb) == -1)
	{
	
	}
	else
	{
		TRACE("load dump.rdb %ld\n",sb.st_size);
	}
}

uint32_t xRdb::digits10(uint64_t v)
{
	if (v < 10) return 1;
    if (v < 100) return 2;
    if (v < 1000) return 3;
    if (v < 1000000000000UL) {
        if (v < 100000000UL) {
            if (v < 1000000) {
                if (v < 10000) return 4;
                return 5 + (v >= 100000);
            }
            return 7 + (v >= 10000000UL);
        }
        if (v < 10000000000UL) {
            return 9 + (v >= 1000000000UL);
        }
        return 11 + (v >= 100000000000UL);
    }
    return 12 + digits10(v / 1000000000000UL);
}

int xRdb::lll2string(char* dst, size_t dstlen, long long svalue)
{
	static const char digits[201] =
	    "0001020304050607080910111213141516171819"
	    "2021222324252627282930313233343536373839"
	    "4041424344454647484950515253545556575859"
	    "6061626364656667686970717273747576777879"
	    "8081828384858687888990919293949596979899";
	int negative;
	unsigned long long value;

	/* The main loop works with 64bit unsigned integers for simplicity, so
	 * we convert the number here and remember if it is negative. */
	if (svalue < 0) {
	    if (svalue != LLONG_MIN) {
	        value = -svalue;
	    } else {
	        value = ((unsigned long long) LLONG_MAX)+1;
	    }
	    negative = 1;
	} else {
	    value = svalue;
	    negative = 0;
	}

	/* Check length. */
	uint32_t const length = digits10(value)+negative;
	if (length >= dstlen) return 0;

	/* Null term. */
	uint32_t next = length;
	dst[next] = '\0';
	next--;
	while (value >= 100) {
	    int const i = (value % 100) * 2;
	    value /= 100;
	    dst[next] = digits[i + 1];
	    dst[next - 1] = digits[i];
	    next -= 2;
	}

	/* Handle last 1-2 digits. */
	if (value < 10) {
	    dst[next] = '0' + (uint32_t) value;
	} else {
	    int i = (uint32_t) value * 2;
	    dst[next] = digits[i + 1];
	    dst[next - 1] = digits[i];
	}

	/* Add sign. */
	if (negative) dst[0] = '-';
	return length;
}


uint32_t xRdb::rdbLoadLen(xRio *rdb, int *isencoded)
{
	unsigned char buf[2];
	uint32_t len;
	int type;
	if(isencoded)
	{
		*isencoded = 0;
	}

	if(rioRead(rdb,buf,1) == 0)
	{
		return REDIS_RDB_LENERR;
	}
	type = (buf[0]&0xC0)>>6;

	if(type == REDIS_RDB_ENCVAL)
	{
		if(isencoded)
		{
			*isencoded = 1;
		}
		return buf[0]&0x3F;
	}
	else if(type == REDIS_RDB_6BITLEN)
	{
		return buf[0]&0x3F;
	}
	else if(type == REDIS_RDB_14BITLEN)
	{
		if (rioRead(rdb,buf+1,1) == 0)
		{
			return REDIS_RDB_LENERR;	
		}
        return ((buf[0]&0x3F)<<8)|buf[1];
	}
	else
	{
		if (rioRead(rdb,&len,4) == 0)
		{
			return REDIS_RDB_LENERR;
		}
        return ntohl(len);
	}
	
}

rObj *xRdb::rdbLoadIntegerObject(xRio *rdb, int enctype, int encode)
{
	unsigned char enc[4];
	long long val;

	if(enctype == REDIS_RDB_ENC_INT8)
	{
		if(rioRead(rdb,enc,1) == 0)
		{
			return nullptr;
		}
		val = (signed char)enc[0];
	}
	else if(enctype == REDIS_RDB_ENC_INT16)
	{
		uint16_t v;
        if (rioRead(rdb,enc,2) == 0) return NULL;
        v = enc[0]|(enc[1]<<8);
        val = (int16_t)v;
	}
	else if (enctype == REDIS_RDB_ENC_INT32)
	{
        uint32_t v;
        if (rioRead(rdb,enc,4) == 0) return NULL;
        v = enc[0]|(enc[1]<<8)|(enc[2]<<16)|(enc[3]<<24);
        val = (int32_t)v;
    } 
	else 
	{
        val = 0; /* anti-warning */
        TRACE("Unknown RDB integer encoding type");
    }

	if (encode)
        return createStringObjectFromLongLong(val);
    else
        return createObject(REDIS_STRING,sdsfromlonglong(val));
	
	
}


rObj *xRdb::rdbLoadEncodedStringObject(xRio *rdb)
{
	return rdbGenericLoadStringObject(rdb,1);
}


rObj *xRdb::rdbGenericLoadStringObject(xRio *rdb, int encode)
{
    int isencoded;
    uint32_t len;
    rObj *o;

    len = rdbLoadLen(rdb,&isencoded);
    if (isencoded) {
        switch(len) {
        case REDIS_RDB_ENC_INT8:
        case REDIS_RDB_ENC_INT16:
        case REDIS_RDB_ENC_INT32:
            return rdbLoadIntegerObject(rdb,len,encode);
        case REDIS_RDB_ENC_LZF:
            //return rdbLoadLzfStringObject(rdb);
        default:
            TRACE("Unknown RDB encoding type");
            break;
        }
    }

    if (len == REDIS_RDB_LENERR)
	{
		return nullptr;
    }
    o = createStringObject(NULL,len);
    if (len && rioRead(rdb,(void*)o->ptr,len) == 0) 
	{
        return nullptr;
    }
    
    return o;
}




rObj *xRdb::rdbLoadStringObject(xRio *rdb)
{
	 return rdbGenericLoadStringObject(rdb,0);
}



int xRdb::rdbLoad(char *filename)
{
	uint32_t dbid;
	int type,rdbver;
	char buf[1024];
	FILE *fp ;
	xRio rdb;

	if((fp = fopen(filename,"r")) == nullptr)
	{
		return REDIS_ERR;
	}

	rioInitWithFile(&rdb,fp);
	if(rioRead(&rdb,buf,9) == 0)
	{
		return REDIS_ERR;
	}
	buf[9] = '\0';

	if(memcmp(buf,"REDIS",5) !=0)
	{
		fclose(fp);
		TRACE("Wrong signature trying to load DB from file");
		return REDIS_ERR;
	}

	struct stat sb;
	if(fstat(fileno(fp),&sb) == -1)
	{
	
	}
	else
	{
		TRACE("load dump.rdb %ld\n",sb.st_size);
	}


	while(1)
	{
		rObj *key,*val;

		if ((type = rdbLoadType(&rdb)) == -1)
		{
			return  REDIS_ERR;
		}

		if (type == REDIS_RDB_OPCODE_EOF)
		{
			break;
		}
		  
		if ((key = rdbLoadStringObject(&rdb)) == nullptr)
		{
			return  REDIS_ERR;
		}
  
        if ((val = rdbLoadObject(type,&rdb)) == nullptr)
        {
        	return REDIS_ERR;
        }
		
		key->calHash();
		val->calHash();
		setMap[key] = val;
	}

	uint64_t cksum,expected = rdb.cksum;
	if(rioRead(&rdb,&cksum,8) == 0)
	{
		return REDIS_ERR;
	}
	
	//memrev64ifbe(&cksum);
	 
	if (cksum == 0)
	{
		TRACE("RDB file was saved with checksum disabled: no check performed");
	}
	else if(cksum != expected)
	{
		TRACE("Wrong RDB checksum. Aborting now");
	}

	fclose(fp);
	return REDIS_OK;
}


int xRdb::rdbLoadType(xRio *rdb) 
{
    unsigned char type;
    if (rioRead(rdb,&type,1) == 0) return -1;
    return type;
}


int xRdb::rdbSaveLen(xRio *rdb, uint32_t len)
{
	unsigned char buf[2];
    size_t nwritten;

    if (len < (1<<6)) {
        /* Save a 6 bit len */
        buf[0] = (len&0xFF)|(REDIS_RDB_6BITLEN<<6);
        if (rdbWriteRaw(rdb,buf,1) == -1) return -1;
        nwritten = 1;
    } else if (len < (1<<14)) {
        /* Save a 14 bit len */
        buf[0] = ((len>>8)&0xFF)|(REDIS_RDB_14BITLEN<<6);
        buf[1] = len&0xFF;
        if (rdbWriteRaw(rdb,buf,2) == -1) return -1;
        nwritten = 2;
    } else {
        /* Save a 32 bit len */
        buf[0] = (REDIS_RDB_32BITLEN<<6);
        if (rdbWriteRaw(rdb,buf,1) == -1) return -1;
        len = htonl(len);
        if (rdbWriteRaw(rdb,&len,4) == -1) return -1;
        nwritten = 1+4;
    }
    return nwritten;
}

int xRdb::rdbSaveRawString(xRio *rdb, const  char *s, size_t len)
{
	int enclen;
	int n,nwritten = 0;
	if((n = rdbSaveLen(rdb,len) == -1))
	{
		return -1;
	}
	nwritten += n;
	if(len > 0)
	{
		if (rdbWriteRaw(rdb,(void*)s,len) == -1) 
		{
			return -1;	
		}
		
        nwritten += len;
	}
	
	return 1;
	
}



rObj *xRdb::rdbLoadObject(int rdbtype, xRio *rdb) 
{
	rObj *o, *ele, *dec;
	size_t len;
	unsigned int i;
	if(rdbtype == REDIS_RDB_TYPE_STRING)
	{
		if ((o = rdbLoadEncodedStringObject(rdb)) == nullptr)
		{
			return nullptr;	
		}
	}
	else
	{
		TRACE("rdbLoadObject type error\n");
	}
	return o;
}

int xRdb::rdbSaveStringObject(xRio *rdb, rObj *obj)
{
	if(obj->encoding == REDIS_ENCODING_INT)
	{
		return rdbSaveLongLongAsStringObject(rdb,(long)obj->ptr);
	}
	else
	{
		return rdbSaveRawString(rdb,obj->ptr,sdsllen(obj->ptr));
	}
	return 1;
}


int xRdb::rdbSaveType(xRio *rdb, unsigned char type)
{
    return rdbWriteRaw(rdb,&type,1);
}



int xRdb::rdbSaveObjectType(xRio *rdb, rObj *o)
{
	switch(o->type)
	{
		case REDIS_STRING:
		{
			return rdbSaveType(rdb,REDIS_RDB_TYPE_STRING);
		}
	}
	return 1;
}	


int xRdb::rdbSaveObject(xRio *rdb, rObj *o)
{
	int n, nwritten = 0;

	if (o->type == REDIS_STRING)
	{
        /* Save a string value */
        if ((n = rdbSaveStringObject(rdb,o)) == -1)
		{
			return -1;
		}
        nwritten += n;
    } 
	else if (o->type == REDIS_LIST)
	{
		
	}
    
	return nwritten;
}

	

int xRdb::rdbSaveKeyValuePair(xRio *rdb, rObj *key, rObj *val, long long now)
{
	if (rdbSaveObjectType(rdb,val) == -1) return -1;
    if (rdbSaveStringObject(rdb,key) == -1) return -1;
    if (rdbSaveObject(rdb,val) == -1) return -1;
	return 1;
}

int xRdb::rdbWriteRaw(xRio *rdb, void *p, size_t len)
{
   if (rdb && rioWrite(rdb,p,len) == 0)
   { 
   	 return -1;
   }
	
   return len;
}


int xRdb::rdbSaveRio(xRio *rdb,int *error)
{
	char magic[10];
	int j;
	long long now = time(0);
	uint64_t cksum;
	snprintf(magic,sizeof(magic),"REDIS%04d",REDIS_RDB_VERSION);
	if (rdbWriteRaw(rdb,magic,9) == -1) 
	{
	   return REDIS_ERR;
	}

	for(auto it = setMap.begin(); it != setMap.end(); it++)
	{
		 if (rdbSaveKeyValuePair(rdb,it->first,it->second,now) == -1)
		 {
		 	return REDIS_ERR;
		 }
	}

	if (rdbSaveType(rdb,REDIS_RDB_OPCODE_EOF) == -1)
	{
		return REDIS_ERR;
	}

	cksum = rdb->cksum;
	//  memrev64ifbe(&cksum);
	if(rioWrite(rdb,&cksum,9) == 0)
	{
		return REDIS_ERR;
	}
	
	return REDIS_OK;
}
int xRdb::rdbSave(char *filename)
{
	char tmpfile[256];
	FILE *fp;
	xRio rdb;
	int error;

	snprintf(tmpfile,256,"temp-%d.rdb",(int)getpid());
	fp = fopen(tmpfile,"w");
	if(!fp)
	{
		 TRACE("Failed opening .rdb for saving: %s",strerror(errno));
		 return REDIS_ERR;
	}

	rioInitWithFile(&rdb,fp);
	if(rdbSaveRio(&rdb,&error) == REDIS_ERR)
	{
		errno = error;
		return REDIS_ERR;
	}

	if (fflush(fp) == EOF) return REDIS_ERR;
    if (fsync(fileno(fp)) == -1) return REDIS_ERR;
    if (fclose(fp) == EOF) return REDIS_ERR;

	if(rename(tmpfile,filename) == -1)
	{
		//_unlink(tmpfile);
		return REDIS_ERR;
	}
	
	return REDIS_OK;
}
