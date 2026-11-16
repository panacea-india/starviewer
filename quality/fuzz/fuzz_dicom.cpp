// libFuzzer harnesses for the third-party DICOM code that Starviewer uses to read files (A-05, H-11).
//
// One source, two programs, chosen at compile time:
//   -DFUZZ_DCMTK   DCMTK: parse the file, walk every element, and decode the first frame.
//   -DFUZZ_GDCM    GDCM: read the file as an image and decode its pixel data. ITK's GDCM reader,
//                  which Starviewer uses, goes through the same gdcm::ImageReader.
//
// The decoders Starviewer registers are registered here too, so the transfer syntaxes of
// the corpus reach the codecs: JPEG, JPEG-LS and RLE for DCMTK, and everything GDCM has.
//
// Images larger than MAX_SIDE or MAX_PIXELS are skipped after parsing. Without that, the
// first findings would be allocations of many gigabytes, and the fuzzer would stop there
// and hide the memory errors. Whether Starviewer should refuse such files is a separate
// question (see README.md).

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#if defined(FUZZ_DCMTK)
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/dcistrmb.h"
#include "dcmtk/dcmdata/dcrledrg.h"
#include "dcmtk/dcmimgle/dcmimage.h"
#include "dcmtk/dcmjpeg/djdecode.h"
#include "dcmtk/dcmjpls/djdecode.h"
#elif defined(FUZZ_GDCM)
#include "gdcmImage.h"
#include "gdcmImageReader.h"
#include "gdcmGlobal.h"
#else
#error "Define FUZZ_DCMTK or FUZZ_GDCM"
#endif

namespace {

constexpr unsigned long MAX_SIDE = 8192;
constexpr unsigned long long MAX_PIXELS = 64ull * 1024 * 1024;
constexpr unsigned long MAX_FRAMES = 512;

bool sizeIsReasonable(unsigned long rows, unsigned long columns, unsigned long frames)
{
    if (rows > MAX_SIDE || columns > MAX_SIDE || frames > MAX_FRAMES)
    {
        return false;
    }
    return static_cast<unsigned long long>(rows) * columns * (frames ? frames : 1) <= MAX_PIXELS;
}

}

#if defined(FUZZ_DCMTK)

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    DJDecoderRegistration::registerCodecs();
    DJLSDecoderRegistration::registerCodecs();
    DcmRLEDecoderRegistration::registerCodecs();
    // Do not let a corrupt file ask for gigabytes of text through the logger.
    OFLog::configure(OFLogger::FATAL_LOG_LEVEL);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    DcmInputBufferStream stream;
    stream.setBuffer(data, static_cast<offile_off_t>(size));
    stream.setEos();

    DcmFileFormat file;
    file.transferInit();
    OFCondition status = file.read(stream);
    file.transferEnd();
    if (status.bad() || file.getDataset() == nullptr)
    {
        return 0;
    }

    DcmDataset *dataset = file.getDataset();

    // Walk every element and format its value, as the tag reader does for the ones it needs.
    std::ostringstream text;
    dataset->print(text, DCMTypes::PF_shortenLongTagValues);

    Uint16 rows = 0;
    Uint16 columns = 0;
    Sint32 frames = 1;
    dataset->findAndGetUint16(DCM_Rows, rows);
    dataset->findAndGetUint16(DCM_Columns, columns);
    dataset->findAndGetSint32(DCM_NumberOfFrames, frames);
    if (frames < 1 || !sizeIsReasonable(rows, columns, static_cast<unsigned long>(frames)))
    {
        return 0;
    }

    DicomImage image(&file, dataset->getOriginalXfer(), CIF_UsePartialAccessToPixelData, 0, 1);
    if (image.getStatus() == EIS_Normal)
    {
        image.getOutputData(8, 0);
        double minimum = 0.0;
        double maximum = 0.0;
        image.getMinMaxValues(minimum, maximum);
    }
    return 0;
}

#elif defined(FUZZ_GDCM)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::istringstream stream(std::string(reinterpret_cast<const char *>(data), size), std::ios::binary);

    // Starviewer's GDCM reader (volumepixeldatareaderitkgdcm.cpp) catches std::bad_alloc,
    // gdcm::Exception and everything else, so exceptions are an expected way to refuse a file.
    // Memory errors are what this harness looks for.
    try
    {
        gdcm::ImageReader reader;
        reader.SetStream(stream);
        if (!reader.Read())
        {
            return 0;
        }

        const gdcm::Image &image = reader.GetImage();
        const unsigned int *dimensions = image.GetDimensions();
        unsigned long frames = image.GetNumberOfDimensions() > 2 ? dimensions[2] : 1;
        if (image.GetNumberOfDimensions() < 2 || !sizeIsReasonable(dimensions[1], dimensions[0], frames))
        {
            return 0;
        }

        std::vector<char> buffer(image.GetBufferLength());
        image.GetBuffer(buffer.data());
    }
    catch (const std::exception &)
    {
    }
    catch (...)
    {
    }
    return 0;
}

#endif
