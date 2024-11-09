#pragma once

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>
#include <AnalyzerHelpers.h>
#include <memory>

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