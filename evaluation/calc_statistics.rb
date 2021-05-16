#!/usr/bin/ruby

require 'fileutils'
require 'wav-file'
require 'pathname'
require 'fileutils'
require 'csv'
require 'narray'
require 'histogram/narray'

# wavデータ取得
def get_wavdata(wavfile_name)
  # データチャンクを取得
  format = nil
  dataChunk = nil
  File.open(wavfile_name) do |file|
    format, chunks = WavFile::readAll(file)
    chunks.each do |chunk|
      dataChunk = chunk if chunk.name == 'data'
    end
  end
  # データを配列化
  case format.bitPerSample
  when 16 then bit = 's*'
  else raise RuntimeError 'Must be 16-bit wav.'
  end
  wavdata = dataChunk.data.unpack(bit) # read binary
  return { 'format' => format, 'data' => wavdata }
end

# 結果ファイル名
RESULT_FILE = "statistics.csv"

# テスト対象の波形が入ったディレクトリリスト
TEST_FILE_DIR_LIST = [
  "./data/**/",
]

# 一時ファイルを削除
FileUtils.rm_rf(RESULT_FILE)

# 圧縮対象のパスをソートしたリスト作成
filelist = []
TEST_FILE_DIR_LIST.each do |dir|
  filelist.append(Dir.glob(File.join(dir, "*.wav")).sort)
end
filelist = filelist.flatten.sort

CSV.open(RESULT_FILE, "w") do |csv|
  # ヘッダ行
  header_line = Array.new
  header_line << "Wave File Name \\ Statistics"
  header_line << "Length[sec]"
  header_line << "RMS[dB]"
  csv << header_line
  # 結果行
  filelist.each do |filename|
    puts filename
    result_line = Array.new
    wavdata = get_wavdata(filename)
    pcm = NArray.to_na(wavdata['data'].map{|d| d.to_f })
    format = wavdata['format']
    pcm *= 2.0 ** -(format.bitPerSample - 1)
    result_line << File.basename(filename)
    result_line << pcm.size.to_f / format.hz
    result_line << 20.0 * Math.log10(pcm.rms)
    csv << result_line
  end
end
