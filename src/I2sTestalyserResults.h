#ifndef I2S_ANALYZER_RESULTS
#define I2S_ANALYZER_RESULTS

#include <AnalyzerResults.h>

class I2sTestalyser;
class I2sTestalyserSettings;

enum I2sResultType
{
    Channel1,
    Channel2,
    ErrorTooFewBits,
    ErrorDoesntDivideEvenly,
    TestError
};


class I2sTestalyserResults : public AnalyzerResults
{
  public:
    I2sTestalyserResults( I2sTestalyser* analyzer, I2sTestalyserSettings* settings );
    virtual ~I2sTestalyserResults();

    virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
    virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

    virtual void GenerateFrameTabularText( U64 frame_index, DisplayBase display_base );
    virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
    virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

  protected: // functions
  protected: // vars
    I2sTestalyserSettings* mSettings;
    I2sTestalyser* mAnalyzer;
};

#endif // I2S_ANALYZER_RESULTS
