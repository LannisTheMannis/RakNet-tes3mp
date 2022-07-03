/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  Copyright (c) 2016-2018, TES3MP Team
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "Lobby2Server_PGSQL.h"
#include "PostgreSQLInterface.h"

using namespace RakNet;

STATIC_FACTORY_DEFINITIONS(Lobby2Server_PGSQL,Lobby2Server_PGSQL);

Lobby2ServerCommand Lobby2ServerWorkerThread(Lobby2ServerCommand input, bool *returnOutput, void* perThreadData)
{
	PostgreSQLInterface *postgreSQLInterface = (PostgreSQLInterface *) perThreadData;
	input.returnToSender = input.lobby2Message->ServerDBImpl(&input, postgreSQLInterface);
	*returnOutput=input.returnToSender;
	if (input.deallocMsgWhenDone && input.returnToSender==false)
		RakNet::OP_DELETE(input.lobby2Message);
	return input;
}

Lobby2Server_PGSQL::Lobby2Server_PGSQL()
{
}
Lobby2Server_PGSQL::~Lobby2Server_PGSQL()
{
	Clear();
}
void Lobby2Server_PGSQL::AddInputFromThread(Lobby2Message *msg, unsigned int targetUserId, RakNet::RakString targetUserHandle)
{
	Lobby2ServerCommand command;
	command.lobby2Message=msg;
	command.deallocMsgWhenDone=true;
	command.returnToSender=true;
	command.callerUserId=targetUserId;
	command.callingUserName=targetUserHandle;
	command.server=this;
	AddInputCommand(command);
}
void Lobby2Server_PGSQL::AddInputCommand(Lobby2ServerCommand command)
{
	threadPool.AddInput(Lobby2ServerWorkerThread, command);
}
void Lobby2Server_PGSQL::AddOutputFromThread(Lobby2Message *msg, unsigned int targetUserId, RakNet::RakString targetUserHandle)
{
	Lobby2ServerCommand command;
	command.lobby2Message=msg;
	command.deallocMsgWhenDone=true;
	command.returnToSender=true;
	command.callerUserId=targetUserId;
	command.callingUserName=targetUserHandle;
	command.server=this;
	msg->resultCode=L2RC_SUCCESS;
	threadPool.AddOutput(command);
}
bool Lobby2Server_PGSQL::ConnectToDB(const char *conninfo, int numWorkerThreads)
{
	if (numWorkerThreads<=0)
		return false;

	StopThreads();

	int i;
	PostgreSQLInterface *connection;
	for (i=0; i < numWorkerThreads; i++)
	{
		connection = RakNet::OP_NEW<PostgreSQLInterface>(  );
		if (connection->Connect(conninfo)==false)
		{
			RakNet::OP_DELETE(connection);
			ClearConnections();
			return false;
		}
		connectionPoolMutex.Lock();
		connectionPool.Insert(connection );
		connectionPoolMutex.Unlock();
	}

	threadPool.SetThreadDataInterface(this,0);
	threadPool.StartThreads(numWorkerThreads,0,0,0);
	return true;
}
void* Lobby2Server_PGSQL::PerThreadFactory(void *context)
{
	(void)context;

	PostgreSQLInterface* p;
	connectionPoolMutex.Lock();
	p=connectionPool.Pop();
	connectionPoolMutex.Unlock();
	return p;
}
void Lobby2Server_PGSQL::PerThreadDestructor(void* factoryResult, void *context)
{
	(void)context;

	PostgreSQLInterface* p = (PostgreSQLInterface*)factoryResult;
	RakNet::OP_DELETE(p);
}
void Lobby2Server_PGSQL::ClearConnections(void)
{
	unsigned int i;
	connectionPoolMutex.Lock();
	for (i=0; i < connectionPool.Size(); i++)
		RakNet::OP_DELETE(connectionPool[i]);
	connectionPool.Clear(false);
	connectionPoolMutex.Unlock();
}
