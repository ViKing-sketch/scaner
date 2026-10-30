#include <fstream>
#include <stdexcept>
#include <vector>
#include <format>

#include <tape_scanner/scanner/DicomFileMeta.hpp>
#include <tape_scanner/scanner/DicomNumbers.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcxfer.h>

#include <tape_scanner/dicom_generator/BinToDcmConverter.hpp>

namespace tape_scanner::dicom_generator{

using std::runtime_error;

void ConvertBinToDcm(string in_bin_file_path, size_t width, size_t height, size_t bits, size_t frames,
                     bool all_frames_as_1st, std::function<void(const string& msg)> log_callback,
                     string_opt out_dcm_file_path) {
  // calculate minimum input file size
  size_t frame_bin_size = width * height * (bits/8);
  size_t src_file_size_min = frame_bin_size * frames;
  log_callback(std::format("Input file minimum size: {} bytes\n", src_file_size_min));

  // setup desitnation file data
  string dest_file_path = out_dcm_file_path.value_or(in_bin_file_path + ".dcm");

  // vars allocation
  char cstr[256];
  E_TransferSyntax writeXfer = EXS_LittleEndianImplicit;
  DcmFileFormat dcm_file;
  DcmDataset* dataset = dcm_file.getDataset();
  DcmMetaInfo* metainfo = dcm_file.getMetaInfo();
  OFCondition condition;
  std::ifstream in;
  size_t frame;
  bool stop;
  size_t src_file_size;

  // open input file
  in.open(in_bin_file_path, std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    throw runtime_error("ERROR: Can't open input file for reading.\n");
  }

  // get input file size
  in.seekg(0, std::ios_base::end);
  src_file_size = in.tellg();
  in.seekg(0);

  log_callback(std::format("Input file size: {} bytes\n", src_file_size));

  if (src_file_size < src_file_size_min) {
    throw runtime_error("ERROR: Input file size is too small.\n");
  }

  // allocate input buffer
  std::vector<Uint16> buf(src_file_size_min / 2);
  auto raw_ptr = reinterpret_cast<char *>(buf.data());

  // setup general info
  // see info here - https://dicom.innolitics.com/ciods/
  metainfo->putAndInsertOFStringArray(DCM_TransferSyntaxUID, UID_LittleEndianImplicitTransferSyntax);

  // setup general tags
  dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
  dataset->putAndInsertOFStringArray(DCM_SeriesDescription, "Converted (BIN/RAW to DICOM)");
  dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, dcmGenerateUniqueIdentifier(cstr, SITE_SERIES_UID_ROOT));
  dataset->putAndInsertOFStringArray(DCM_Modality, "XC"); // External-camera Photography
  dataset->putAndInsertOFStringArray(DCM_ConversionType, "SI"); // Scanned Image
  dataset->putAndInsertOFStringArray(DCM_PatientName, "BIN^TO^DICOM");
  dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(cstr, SITE_INSTANCE_UID_ROOT));
  dataset->putAndInsertOFStringArray(DCM_StudyID, dcmGenerateUniqueIdentifier(cstr, SITE_STUDY_UID_ROOT));
  dataset->putAndInsertOFStringArray(DCM_BurnedInAnnotation, "NO");
  const ScanGeometry nominal_geometry;
  const std::string nominal_spacing = FormatDicomDecimal(nominal_geometry.RowSpacingMm()) + "\\" +
                                      FormatDicomDecimal(nominal_geometry.ColSpacingMm());
  dataset->putAndInsertString(DCM_NominalScannedPixelSpacing, nominal_spacing.c_str());
  dataset->putAndInsertString(DCM_PixelSpacing, nominal_spacing.c_str());
  dataset->putAndInsertString(DCM_PixelAspectRatio, kDicomPixelAspectRatio);
  dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
  dataset->putAndInsertOFStringArray(DCM_RescaleIntercept, "0");
  dataset->putAndInsertOFStringArray(DCM_RescaleSlope, "1");
  dataset->putAndInsertOFStringArray(DCM_RescaleType, "US");
  dataset->putAndInsertOFStringArray(DCM_PresentationLUTShape, "IDENTITY");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
  dataset->putAndInsertUint16(DCM_BitsAllocated, bits > 8 ? 16 : 8);
  dataset->putAndInsertUint16(DCM_BitsStored, bits);
  dataset->putAndInsertUint16(DCM_HighBit, bits-1);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
  dataset->putAndInsertUint16(DCM_Rows, height);
  dataset->putAndInsertUint16(DCM_Columns, width);

  // setup multi-frame DICOM file
  if (frames > 1) {
    // setup storage
    dataset->putAndInsertOFStringArray(DCM_SOPClassUID, (bits > 8) ?
                                                            UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage :
                                                            UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage);
    // setup frames
    OFStandard::snprintf(cstr, 30, "%zu", frames);
    dataset->putAndInsertOFStringArray(DCM_NumberOfFrames, cstr);
    dataset->putAndInsertOFStringArray(DCM_FrameTime, "1000");
    dataset->putAndInsertTagKey(DCM_FrameIncrementPointer, DCM_FrameTime);
    // if all frames must be a copy of 1st frame
    if (all_frames_as_1st) {
      for (frame = 0; frame < frames; frame++) {
        // read input file into buffer
        in.seekg(0);
        if (in.read(raw_ptr + frame_bin_size * frame, frame_bin_size).bad()) {
          throw runtime_error("ERROR: Failed to read input file into RAM buffer.\n");
        }
      }
      // all frames must be unique
    } else {
      // read whole input file into buffer
      if (in.read(raw_ptr, src_file_size_min).bad()) {
        throw runtime_error("ERROR: Failed to read whole input file into RAM buffer.\n");
      }
    }
    // setup single-frame DICOM file
  } else {
    // setup storage
    dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
    // read whole input file into buffer
    if (in.read(raw_ptr, src_file_size_min).bad()) {
      throw runtime_error("ERROR: Failed to read whole input file into RAM buffer.\n");
    }
  }
  // insert frames binary data
  dataset->putAndInsertUint8Array(DCM_PixelData, reinterpret_cast<Uint8 *>(raw_ptr), src_file_size_min);
  MatchDicomMediaStorage(metainfo, dataset);
  condition = dcm_file.saveFile((const OFFilename)dest_file_path.c_str(),
                                writeXfer, EET_ExplicitLength, EGL_recalcGL, EPD_noChange,
                                OFstatic_cast(Uint32, 0), OFstatic_cast(Uint32, 0), EWM_updateMeta);
  if (condition.bad()) {
    throw runtime_error("ERROR: Failed to save resulting DICOM file.\n");
  }
}

} // namespace tape_scanner::dicom_generator
