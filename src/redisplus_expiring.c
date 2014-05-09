/*
 * myredis-expiring.c
 *
 *  Created on: 2014. 5. 8.
 *      Author: powerumc
 */

#include "redisplus_expiring.h"

char* itoa(int val, int base){

	static char buf[32] = {0};
	int i = 30;

	for(; val && i ; --i, val /= base)
		buf[i] = "0123456789abcdef"[val % base];
	return &buf[i+1];
}

robj *lookupKey_no_expire(redisDb *db, robj *key) {
    dictEntry *de = dictFind(db->dict,key->ptr);
    if (de) {
        robj *val = dictGetVal(de);
        return val;
    } else {
        return NULL;
    }
}

robj *lookupKeyRead_no_expire(redisDb *db, robj *key) {
    robj *val;

    val = lookupKey_no_expire(db,key);
    if (val == NULL)
        server.stat_keyspace_misses++;
    else
        server.stat_keyspace_hits++;
    return val;
}
robj *lookupKeyReadOrReply_no_expire(redisClient *c, robj *key, robj *reply) {
    robj *o = lookupKeyRead_no_expire(c->db, key);
    if (!o) addReply(c,reply);
    return o;
}

void getneCommand(redisClient *c) {
	getGenericCommand_no_exire(c);
}

int getGenericCommand_no_exire(redisClient *c) {
    robj *o;

    if ((o = lookupKeyReadOrReply_no_expire(c,c->argv[1],shared.nullbulk)) == NULL)
        return REDIS_OK;

    if (o->type != REDIS_STRING) {
        addReply(c,shared.wrongtypeerr);
        return REDIS_ERR;
    } else {
        addReplyBulk(c,o);
        return REDIS_OK;
    }
}

void notifyKeyspaceExpiringEvent(int type, char *event, robj *key, robj *val, int dbid) {
    sds chan;
    robj *chanobj, *eventobj;
    int len = -1;
    char buf[24];

    /* If notifications for this class of events are off, return ASAP. */
    if (!(server.notify_keyspace_events & type)) return;

    eventobj = createStringObject(event,strlen(event));

    if (server.notify_keyspace_events & REDIS_NOTIFY_KEYEVENT) {
        chan = sdsnewlen("__keyevent@",11);
        if (len == -1) len = ll2string(buf,sizeof(buf),dbid);
        chan = sdscatlen(chan, buf, len);
        chan = sdscatlen(chan, "__:", 3);
        chan = sdscatsds(chan, eventobj->ptr);
        chanobj = createObject(REDIS_STRING, chan);
        pubsubPublishMessageKeyValue(chanobj, key, val);
        decrRefCount(chanobj);
    }
    decrRefCount(eventobj);
}

int pubsubPublishMessageKeyValue(robj *channel, robj *key, robj *val) {
	int receivers = 0;
	struct dictEntry *de;
	listNode *ln;
	listIter li;
	int fd, iscb = 0;

	/* Send to clients listening for that channel */
	de = dictFind(server.pubsub_channels,channel);
	if (de) {
		list *list = dictGetVal(de);
		listNode *ln;
		listIter li;

		listRewind(list,&li);
		while ((ln = listNext(&li)) != NULL) {
			redisClient *c = ln->value;

			addReply(c,shared.mbulkhdr[4]);
			addReply(c,shared.messagebulk);
			addReplyBulk(c,channel);
			addReplyBulk(c,key);
			addReplyBulk(c,val);
			receivers++;
		}
	}
	/* Send to clients listening to matching channels */
	if (listLength(server.pubsub_patterns)) {
		listRewind(server.pubsub_patterns,&li);
		channel = getDecodedObject(channel);
		while ((ln = listNext(&li)) != NULL) {
			pubsubPattern *pat = ln->value;

			if (stringmatchlen((char*)pat->pattern->ptr,
								sdslen(pat->pattern->ptr),
								(char*)channel->ptr,
								sdslen(channel->ptr),0)) {
				addReply(pat->client,shared.mbulkhdr[5]);
				addReply(pat->client,shared.pmessagebulk);
				addReplyBulk(pat->client,pat->pattern);
				addReplyBulk(pat->client,channel);
				addReplyBulk(pat->client,key);
				addReplyBulk(pat->client,val);
				receivers++;
			}
		}
		decrRefCount(channel);
	}

	return receivers;
}
