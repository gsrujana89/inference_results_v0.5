#ifndef ENTROPY_CALIBRATOR_H
#define ENTROPY_CALIBRATOR_H

#include "NvInfer.h"
#include "BatchStreamPPM.h"

//! \class EntropyCalibratorImpl
//! 
//! \brief Implements common functionality for Entropy calibrators.
//!
class EntropyCalibratorImpl
{
public:
     EntropyCalibratorImpl(BatchStream& stream, int firstBatch, std::string networkName, const char* inputBlobName, bool readCache = true)
        : mStream(stream)
        , mCalibrationTableName("CalibrationTable"+networkName)
        , mInputBlobName(inputBlobName)
        , mReadCache(readCache)
    {
        nvinfer1::DimsNCHW dims = mStream.getDims();
        mInputCount = samplesCommon::volume(dims);
        CHECK(cudaMalloc(&mDeviceInput, mInputCount * sizeof(float)));
        mStream.reset(firstBatch);
    }

    virtual ~EntropyCalibratorImpl()
    {
        CHECK(cudaFree(mDeviceInput));
    }

    int getBatchSize() const { return mStream.getBatchSize(); }

    bool getBatch(void* bindings[], const char* names[], int nbBindings)
    {
        if (!mStream.next())
        {
            return false;
        }
        CHECK(cudaMemcpy(mDeviceInput, mStream.getBatch(), mInputCount * sizeof(float), cudaMemcpyHostToDevice));
        assert(!strcmp(names[0], mInputBlobName));
        bindings[0] = mDeviceInput;
        return true;
    }

    const void* readCalibrationCache(size_t& length)
    {
        mCalibrationCache.clear();
        std::ifstream input(mCalibrationTableName, std::ios::binary);
        input >> std::noskipws;
        if (mReadCache && input.good())
        {
            std::copy(std::istream_iterator<char>(input), std::istream_iterator<char>(), std::back_inserter(mCalibrationCache));
        }
        length = mCalibrationCache.size();
        return length ? mCalibrationCache.data() : nullptr;
    }

    void writeCalibrationCache(const void* cache, size_t length)
    {
        std::ofstream output(mCalibrationTableName, std::ios::binary);
        output.write(reinterpret_cast<const char*>(cache), length);
    }

private:
    BatchStream mStream;
    size_t mInputCount;
    std::string mCalibrationTableName;
    const char* mInputBlobName;
    bool mReadCache{true};
    void* mDeviceInput{nullptr};
    std::vector<char> mCalibrationCache;
};

//! \class Int8EntropyCalibrator2
//! 
//! \brief Implements Entropy calibrator 2. 
//!  CalibrationAlgoType is kENTROPY_CALIBRATION_2.
//!
class Int8EntropyCalibrator2 : public IInt8EntropyCalibrator2
{
public:
    Int8EntropyCalibrator2(BatchStream& stream, int firstBatch, const char* networkName, const char* inputBlobName, bool readCache = true)
        : mImpl(stream, firstBatch, networkName, inputBlobName, readCache)
    {
    }
    
    int getBatchSize() const override { return mImpl.getBatchSize(); }

    bool getBatch(void* bindings[], const char* names[], int nbBindings) override
    {
        return mImpl.getBatch(bindings, names, nbBindings);
    }

    const void* readCalibrationCache(size_t& length) override
    {
        return mImpl.readCalibrationCache(length);
    }
    
    void writeCalibrationCache(const void* cache, size_t length) override
    {
        mImpl.writeCalibrationCache(cache, length);
    }
private:
    EntropyCalibratorImpl mImpl;
};

#endif // ENTROPY_CALIBRATOR_H
