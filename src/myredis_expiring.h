/*
 * myredis-expiring.h
 *
 *  Created on: 2014. 5. 8.
 *      Author: powerumc
 */

#ifndef MYREDIS_EXPIRING_H_
#define MYREDIS_EXPIRING_H_

#include "redis.h"
#include "sds.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void getneCommand(redisClient *c);
int getGenericCommand_no_exire(redisClient *c);
void notifyKeyspaceExpiringEvent(int type, char *event, robj *key, robj *val, int dbid);
int pubsubPublishMessageKeyValue(robj *channel, robj *key, robj *val);


#endif /* MYREDIS_EXPIRING_H_ */
