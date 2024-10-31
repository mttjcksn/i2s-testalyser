#include "I2sTestalyser.h"
#include "I2sTestalyserSettings.h"
#include <AnalyzerChannelData.h>

I2sTestalyser::I2sTestalyser() : Analyzer2(), mSettings( new I2sTestalyserSettings() ), mSimulationInitilized( false )
{
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

I2sTestalyser::~I2sTestalyser()
{
    KillThread();
}

void I2sTestalyser::SetupResults()
{
    mResults.reset( new I2sTestalyserResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings->mDataChannel );
}

void I2sTestalyser::WorkerThread()
{
    // UpArrow, DownArrow
    if( mSettings->mDataValidEdge == AnalyzerEnums::NegEdge )
        mArrowMarker = AnalyzerResults::DownArrow;
    else
        mArrowMarker = AnalyzerResults::UpArrow;

    mClock = GetAnalyzerChannelData( mSettings->mClockChannel );
    mFrame = GetAnalyzerChannelData( mSettings->mFrameChannel );
    mData = GetAnalyzerChannelData( mSettings->mDataChannel );

    SetupForGettingFirstBit();
    SetupForGettingFirstFrame();

    for( ;; )
    {
        GetFrame();
        AnalyzeFrame();

        mResults->CommitResults();
        ReportProgress( mClock->GetSampleNumber() );
        CheckIfThreadShouldExit();
    }
}

// enum PcmFrameType { FRAME_TRANSITION_TWICE_EVERY_WORD, FRAME_TRANSITION_ONCE_EVERY_WORD, FRAME_TRANSITION_TWICE_EVERY_FOUR_WORDS };
void I2sTestalyser::AnalyzeFrame()
{
    U32 num_bits = mDataBits.size();

    U32 num_frames = 0;
    switch( mSettings->mFrameType )
    {
    case FRAME_TRANSITION_TWICE_EVERY_WORD:
        num_frames = 1;
        break;
    case FRAME_TRANSITION_ONCE_EVERY_WORD:
        num_frames = 2;
        break;
    case FRAME_TRANSITION_TWICE_EVERY_FOUR_WORDS:
        num_frames = 4;
        break;
    default:
        AnalyzerHelpers::Assert( "unexpected" );
        break;
    }

    if( ( num_bits % num_frames ) != 0 )
    {
        Frame frame;
        frame.mType = U8( ErrorDoesntDivideEvenly );
        frame.mFlags = DISPLAY_AS_ERROR_FLAG;
        frame.mStartingSampleInclusive = mDataValidEdges.front();
        frame.mEndingSampleInclusive = mDataValidEdges.back();
        mResults->AddFrame( frame );
        FrameV2 frame_v2;
        frame_v2.AddString( "error", "invalid number of bits" );
        mResults->AddFrameV2( frame_v2, "error", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
        return;
    }

    U32 bits_per_frame = num_bits / num_frames;
    U32 num_audio_bits = mSettings->mBitsPerWord;

    if( bits_per_frame < num_audio_bits )
    {
        // enum I2sResultType { Channel1, Cahannel2, ErrorTooFewBits, ErrorDoesntDivideEvenly };
        Frame frame;
        frame.mType = U8( ErrorTooFewBits );
        frame.mFlags = DISPLAY_AS_ERROR_FLAG;
        frame.mStartingSampleInclusive = mDataValidEdges.front();
        frame.mEndingSampleInclusive = mDataValidEdges.back();
        mResults->AddFrame( frame );
        FrameV2 frame_v2;
        frame_v2.AddString( "error", "not enough bits" );
        mResults->AddFrameV2( frame_v2, "error", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
        return;
    }

    U32 num_unused_bits = bits_per_frame - num_audio_bits;
    U32 starting_offset;

    if( mSettings->mWordAlignment == LEFT_ALIGNED )
        starting_offset = 0;
    else
        starting_offset = num_unused_bits;

    for( U32 i = 0; i < num_frames; i++ )
    {
        AnalyzeSubFrame( i * bits_per_frame + starting_offset, num_audio_bits, i );
    }
}

void I2sTestalyser::AnalyzeSubFrame( U32 starting_index, U32 num_bits, U32 subframe_index )
{
    U64 result = 0;
    U32 target_count = starting_index + num_bits;

    if( mSettings->mShiftOrder == AnalyzerEnums::LsbFirst )
    {
        U64 bit_value = 1ULL;
        for( U32 i = starting_index; i < target_count; i++ )
        {
            if( mDataBits[ i ] == BIT_HIGH )
                result |= bit_value;

            bit_value <<= 1;
        }
    }
    else
    {
        U64 bit_value = 1ULL << ( num_bits - 1 );
        for( U32 i = starting_index; i < target_count; i++ )
        {
            if( mDataBits[ i ] == BIT_HIGH )
                result |= bit_value;

            bit_value >>= 1;
        }
    }

    // enum I2sResultType { Channel1, Channel2, ErrorTooFewBits, ErrorDoesntDivideEvenly };
    // add result bubble
    Frame frame;
    frame.mData1 = result;


    U32 channel_1_polarity = 1;
    if( mSettings->mWordSelectInverted == WS_INVERTED )
        channel_1_polarity = 0;

    if( ( subframe_index & 0x1 ) == channel_1_polarity )
        frame.mType = U8( Channel1 );
    else
        frame.mType = U8( Channel2 );

    frame.mFlags = 0;
    frame.mStartingSampleInclusive = mDataValidEdges[ starting_index ];
    frame.mEndingSampleInclusive = mDataValidEdges[ starting_index + num_bits - 1 ];
    mResults->AddFrame( frame );
    FrameV2 frame_v2;
    frame_v2.AddInteger( "channel", frame.mType );
    S64 adjusted_value = result;
    if( mSettings->mSigned == AnalyzerEnums::SignedInteger )
    {
        adjusted_value = AnalyzerHelpers::ConvertToSignedNumber( frame.mData1, mSettings->mBitsPerWord );
    }
    frame_v2.AddInteger( "data", adjusted_value );
    mResults->AddFrameV2( frame_v2, "data", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
}

void I2sTestalyser::SetupForGettingFirstFrame()
{
    GetNextBit( mLastData, mLastFrame, mLastSample ); // we have to throw away one bit to get enough history on the FRAME line.

    for( ;; )
    {
        GetNextBit( mCurrentData, mCurrentFrame, mCurrentSample );

        if( mCurrentFrame == BIT_HIGH && mLastFrame == BIT_LOW )
        {
            if( mSettings->mBitAlignment == BITS_SHIFTED_RIGHT_1 )
            {
                // we need to advance to the next bit past the frame.
                mLastFrame = mCurrentFrame;
                mLastData = mCurrentData;
                mLastSample = mCurrentSample;

                GetNextBit( mCurrentData, mCurrentFrame, mCurrentSample );
            }
            return;
        }

        mLastFrame = mCurrentFrame;
        mLastData = mCurrentData;
        mLastSample = mCurrentSample;
    }
}

void I2sTestalyser::GetFrame()
{
    // on entering this function:
    // mCurrentFrame and mCurrentData are the values of the first bit -- that belongs to us -- in the frame.
    // mLastFrame and mLastData are the values from the bit just before.

    mDataBits.clear();
    mDataValidEdges.clear();


    mDataBits.push_back( mCurrentData );
    mDataValidEdges.push_back( mCurrentSample );

    mLastFrame = mCurrentFrame;
    mLastData = mCurrentData;
    mLastSample = mCurrentSample;

    for( ;; )
    {
        GetNextBit( mCurrentData, mCurrentFrame, mCurrentSample );

        if( mCurrentFrame == BIT_HIGH && mLastFrame == BIT_LOW )
        {
            if( mSettings->mBitAlignment == BITS_SHIFTED_RIGHT_1 )
            {
                // this bit belongs to us:
                mDataBits.push_back( mCurrentData );
                mDataValidEdges.push_back( mCurrentSample );

                // we need to advance to the next bit past the frame.
                mLastFrame = mCurrentFrame;
                mLastData = mCurrentData;
                mLastSample = mCurrentSample;

                GetNextBit( mCurrentData, mCurrentFrame, mCurrentSample );
            }

            return;
        }

        mDataBits.push_back( mCurrentData );
        mDataValidEdges.push_back( mCurrentSample );

        mLastFrame = mCurrentFrame;
        mLastData = mCurrentData;
        mLastSample = mCurrentSample;
    }
}

void I2sTestalyser::SetupForGettingFirstBit()
{
    if( mSettings->mDataValidEdge == AnalyzerEnums::PosEdge )
    {
        // we want to start out low, so the next time we advance, it'll be a rising edge.
        if( mClock->GetBitState() == BIT_HIGH )
            mClock->AdvanceToNextEdge(); // now we're low.
    }
    else
    {
        // we want to start out low, so the next time we advance, it'll be a falling edge.
        if( mClock->GetBitState() == BIT_LOW )
            mClock->AdvanceToNextEdge(); // now we're high.
    }
}

void I2sTestalyser::GetNextBit( BitState& data, BitState& frame, U64& sample_number )
{
    // we always start off here so that the next edge is where the data is valid.
    mClock->AdvanceToNextEdge();
    U64 data_valid_sample = mClock->GetSampleNumber();

    mData->AdvanceToAbsPosition( data_valid_sample );
    data = mData->GetBitState();

    mFrame->AdvanceToAbsPosition( data_valid_sample );
    frame = mFrame->GetBitState();

    sample_number = data_valid_sample;

    mResults->AddMarker( data_valid_sample, mArrowMarker, mSettings->mClockChannel );

    mClock->AdvanceToNextEdge(); // advance one more, so we're ready for next this function is called.
}

U32 I2sTestalyser::GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels )
{
    if( mSimulationInitilized == false )
    {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( newest_sample_requested, sample_rate, simulation_channels );
}

U32 I2sTestalyser::GetMinimumSampleRateHz()
{
    return 4000000; // just enough for our simulation.  Ideally we would be smarter about this but we don't know the bit rate in advance.
}

bool I2sTestalyser::NeedsRerun()
{
    return false;
}

const char* I2sTestalyser::GetAnalyzerName() const
{
    return "I2S / PCM Test";
}

const char* GetAnalyzerName()
{
    return "I2S / PCM Test";
}

Analyzer* CreateAnalyzer()
{
    return new I2sTestalyser();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}
