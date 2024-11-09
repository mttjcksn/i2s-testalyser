#pragma once

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>
#include <AnalyzerHelpers.h>
#include "TestServer.hpp"

#include <memory>
#include <algorithm>
#include <string>
#include <limits>


enum TestMode
{
    TEST_DISABLED,
    TEST_CONTIGUOUS
};

// Note: We give away the pointer to the setting interfaces, so we need to ensure that they are not used after the TestExtensionSettings is
// deleted..
//       Perhaps there is a better way.

class TestExtensionSettings
{
  public:
    TestExtensionSettings() : mTestMode( TEST_DISABLED ), mUseTestServer( false )
    {
        mTestModeInterface.reset( new AnalyzerSettingInterfaceNumberList() );
        mTestModeInterface->SetTitleAndTooltip( "Test mode", "Select a data test. Results will show test errors." );
        mTestModeInterface->AddNumber( TEST_DISABLED, "No test", "normal analyser operation." );
        mTestModeInterface->AddNumber( TEST_CONTIGUOUS, "Contiguous", "Reports errors if channel samples are not contiguous." );
        mTestModeInterface->SetNumber( mTestMode );

        mUseTestServerInterface.reset( new AnalyzerSettingInterfaceBool() );
        mUseTestServerInterface->SetTitleAndTooltip( "Use test server",
                                                     "Use the custom i2s test server to log clock stats and control automation" );
        mUseTestServerInterface->SetValue( mUseTestServer );
    };

    ~TestExtensionSettings() = default;

    std::vector<AnalyzerSettingInterface*> getSettingInterfaces()
    {
        std::vector<AnalyzerSettingInterface*> interfaces;
        interfaces.push_back( mTestModeInterface.get() );
        interfaces.push_back( mUseTestServerInterface.get() );
        return interfaces;
    }

    void UpdateInterfacesFromSettings()
    {
        mTestModeInterface->SetNumber( mTestMode );
        mUseTestServerInterface->SetValue( mUseTestServer );
    }

    void SetSettingsFromInterfaces()
    {
        mTestMode = TestMode( U32( mTestModeInterface->GetNumber() ) );
        mUseTestServer = mUseTestServerInterface->GetValue();
    }

    void LoadSettings( SimpleArchive& text_archive )
    {
        TestMode test_mode;
        if( text_archive >> *( U32* )&test_mode )
        {
            mTestMode = test_mode;
        }

        bool test_server;
        if( text_archive >> *( U32* )&test_server )
        {
            mUseTestServer = test_server;
        }
    }

    void SaveSettings( SimpleArchive& text_archive )
    {
        text_archive << mTestMode;
        text_archive << mUseTestServer;
    }

    TestMode mTestMode;
    bool mUseTestServer;

    std::unique_ptr<AnalyzerSettingInterfaceNumberList> mTestModeInterface;
    std::unique_ptr<AnalyzerSettingInterfaceBool> mUseTestServerInterface;
};

class TestExtension
{
  public:
    TestExtension() {};

    ~TestExtension() = default;

    void setup( int channelCount, TestExtensionSettings& pSettings )
    {
        mTestChannelPrimed.clear();
        mTestExpectedResults.clear();
        mTestChannelPrimed.assign( channelCount, false );
        mTestExpectedResults.assign( channelCount, 0 );

        mClockMinInterval = std::numeric_limits<U64>::max();
        mClockMaxInterval = 0;
        mStatsUpdateCount = 0;

        if( pSettings.mUseTestServer && !mTestServerConnected )
        {
            mTestServerConnected = mTestServer.connect();
        }
    }

    /**
     * @brief
     *
     * @param pSettings
     * @param subframeIndex
     * @param channel
     * @param value
     * @return true if error
     * @return false if no error
     */
    bool process( TestExtensionSettings& settings, int channel, int value )
    {
        if( settings.mTestMode != TestMode::TEST_CONTIGUOUS )
        {
            return false;
        }

        if( value != ( mTestExpectedResults.at( channel ) ) )
        {
            if( mTestChannelPrimed.at( channel ) )
            {
                // The test is now unpredictable, allow the next sample to reset the test position
                mTestChannelPrimed.at( channel ) = false;
                mTestServer.error();
                return true;
            }
            else
            {
                // We now have 1 samples to test against
                mTestChannelPrimed.at( channel ) = true;
            }
        }
        else
        {
            // We now have 1 samples to test against
            mTestChannelPrimed.at( channel ) = true;
        }

        mTestExpectedResults.at( channel ) = value + 1;

        return false;
    }

    void setDataValidEdge( uint64_t sampleNumber )
    {
        // We want to detect movements in the clock of +/- 1ppm.
        // With a analyser sample rate of 500000000, that requires timing the clock edges 500 times per second.
        // With a sample rate of 48000, 500 times per second equates to once every 96 frames: 500000000 / (48000/96).
        // Therefore, we output the delta between the  measured number of samples between FS clk edge every 96 samples, and 1000000.

        if(edgeCount >= (96*32*2)-1)
        {
            U64 clockDelta = sampleNumber - mDataValidEdgeSample;
            mDataValidEdgeSample = sampleNumber;
            edgeCount = 0;

            mClockMinInterval = std::min( mClockMinInterval, clockDelta );
            mClockMaxInterval = std::max( mClockMaxInterval, clockDelta );
            mStatsUpdateCount++;
            if( mStatsUpdateCount >= mStatsUpdateInterval )
            {
                mStatsUpdateCount = 0;
                if( mTestServerConnected )
                {
                    mTestServer.update( mClockMinInterval, mClockMaxInterval );
                }
                mClockMinInterval = std::numeric_limits<U64>::max();
                mClockMaxInterval = 0;
            }
        }
        else
        {
            edgeCount++;
        }
    }

    void setDataTransitionEdge( uint64_t sampleNumber )
    {
    }

  protected:
    U64 mClockMinInterval = std::numeric_limits<U64>::max();
    U64 mClockMaxInterval = 0;
    U32 mStatsUpdateInterval = 500;
    U32 mStatsUpdateCount = 0;
    std::vector<U32> mTestExpectedResults;
    std::vector<bool> mTestChannelPrimed;
    TestServer mTestServer;
    bool mTestServerConnected = false;

    uint64_t mDataValidEdgeSample = 0;
    uint32_t edgeCount = 0;
};