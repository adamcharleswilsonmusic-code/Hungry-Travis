// SPDX-License-Identifier: GPL-3.0-only
// Offline validation of a saved USB-audio track. Opens no capture device.
import AVFoundation
import Foundation
let file = CommandLine.arguments[1]
let asset = AVURLAsset(url: URL(fileURLWithPath: file))
guard let track = asset.tracks(withMediaType: .audio).first else { fatalError("No audio track") }
let reader = try AVAssetReader(asset: asset)
let output = AVAssetReaderTrackOutput(track: track, outputSettings: [
 AVFormatIDKey: kAudioFormatLinearPCM, AVLinearPCMBitDepthKey: 16,
 AVLinearPCMIsFloatKey: false, AVLinearPCMIsBigEndianKey: false,
 AVLinearPCMIsNonInterleaved: false
])
reader.add(output)
guard reader.startReading() else { fatalError("Cannot read saved audio") }
var count = 0, peak = 0, squares = 0.0
while let sample = output.copyNextSampleBuffer() {
 guard let block = CMSampleBufferGetDataBuffer(sample) else { continue }
 var pointer: UnsafeMutablePointer<Int8>?
 var length = 0
 guard CMBlockBufferGetDataPointer(block, atOffset: 0, lengthAtOffsetOut: nil,
                                  totalLengthOut: &length, dataPointerOut: &pointer) == kCMBlockBufferNoErr,
       let pointer = pointer else { fatalError("Cannot read PCM buffer") }
 let bytes = UnsafeRawPointer(pointer)
 for i in stride(from: 0, to: length - 1, by: 2) {
  let value = Int(Int16(littleEndian: bytes.loadUnaligned(fromByteOffset: i, as: Int16.self)))
  peak = max(peak, abs(value)); squares += Double(value) * Double(value); count += 1
 }
}
guard reader.status == .completed && count > 0 else { fatalError("Audio read failed") }
let rms = sqrt(squares / Double(count))
let report: [String: Any] = ["sample_values": count, "peak_pcm16": peak,
 "rms_pcm16": rms, "rms_dbfs": 20 * log10(max(rms, 1e-9) / 32768),
 "audible_signal_present": peak > 100, "scope": "Offline PCM analysis of the actual saved USB audio track"]
print(String(data: try JSONSerialization.data(withJSONObject: report, options: [.sortedKeys]), encoding: .utf8)!)
