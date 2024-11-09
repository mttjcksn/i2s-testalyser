#pragma once

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TEST_SERVER_PORT 65432
#define TEST_SERVER_IP "127.0.0.1"

class TestServer
{
  public:
    TestServer()
    {
    }
    ~TestServer() {};

    bool connect()
    {
        if(mSocket > 0) return true;

        mSocket = socket(AF_INET, SOCK_STREAM, 0);
        if(mSocket < 0)
        {
            mErrorString = "Failed to create socket";
        }

        mServerAddress.sin_family = AF_INET;
        mServerAddress.sin_port = htons(TEST_SERVER_PORT);

        // Convert IP address to binary form
        if(inet_pton( AF_INET, TEST_SERVER_IP, &mServerAddress.sin_addr ) <= 0)
        {
            mErrorString = "Invalid server IP address";
            return false;
        }

        // Attempt to connect to the server
        if(::connect( mSocket, ( struct sockaddr* )&mServerAddress, sizeof( mServerAddress )) < 0)
        {
            mErrorString = "Connection to server failed";
            return false;
        }


        return update(0,1234);
    }

    void disconnect()
    {
        if( mSocket >= 0 )
        {
            close( mSocket );
            mSocket = -1;
        }
    }

    bool update(uint64_t val1, uint64_t val2)
    {
        if(mSocket < 0){ return false; }

        std::string msg = serialise(val1, val2);

        if (send(mSocket, msg.c_str(), msg.size(), 0) < 0)
        {
            mErrorString = "Failed to send message to server";
            return false;
        }
        return true;
    }

    bool error()
    {
        if(mSocket < 0){ return false; }

        std::string msg = serialise(1111, 9999);

        if (send(mSocket, msg.c_str(), msg.size(), 0) < 0)
        {
            mErrorString = "Failed to send message to server";
            return false;
        }
        return true;
    }

  private:
    int mSocket = -1;
    struct sockaddr_in mServerAddress;
    std::string mErrorString = "";

    std::string serialise(uint64_t value1, uint64_t value2)
    {
        std::string result(16, '\0');  // 16 bytes (8 bytes for each uint64_t)
        
        // Store value1 in the first 8 bytes
        for (int i = 0; i < 8; ++i)
        {
            result[i] = static_cast<char>((value1 >> (8 * i)) & 0xFF);
        }
        
        // Store value2 in the next 8 bytes
        for (int i = 0; i < 8; ++i)
        {
            result[i + 8] = static_cast<char>((value2 >> (8 * i)) & 0xFF);
        }
        
        return result;
    }
};