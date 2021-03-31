#!/usr/bin/ruby

require 'fileutils'
require 'wav-file'
require 'benchmark'
require 'pathname'
require 'fileutils'
require 'csv'

class Compressor

    def initialize(tool_name, compress_option_string)
        @tool_name                = tool_name
        @compress_option_string   = compress_option_string
    end

    # コマンドを実行し、実行時間を返す
    def execute_command(command_string)
        result = Benchmark.measure do
            system(command_string)
        end
        # 子プロセスのユーザCPU時間 + システムCPU時間をmsに換算
        1000.0 * (result.cstime + result.cutime)
    end

    # 圧縮設定の文字列を返す
    def get_compress_configre_string
        if @compress_option_string != ""
            return @tool_name + " " + @compress_option_string
        else
            return @tool_name
        end
    end

    # 圧縮処理を実行
    def compress(in_filename, out_filename)
        command = nil
        escaped_in_filename   = "\"#{in_filename}\""
        escaped_out_filename  = "\"#{out_filename}\""
        case @tool_name
        when "flac"
            command = "flac #{@compress_option_string} -f -s -o #{escaped_out_filename} #{escaped_in_filename}"
        when "wavpack"
            command = "wavpack #{@compress_option_string} -q -y #{escaped_in_filename} -o #{escaped_out_filename}"
        when "tta"
            command = "tta -e #{escaped_in_filename} #{escaped_out_filename} 2> /dev/null"
        when "monkey's audio"
            command = "mac #{escaped_in_filename} #{escaped_out_filename} #{@compress_option_string} > /dev/null"
        when "optimfrog"
            command = "ofr --encode #{@compress_option_string} #{escaped_in_filename} --output #{escaped_out_filename} > /dev/null 2>&1"
        when "mp4als"
            # 入力文字列に日本語が含まれているとだめなのでローカルにwavをコピー
            mp4_in_filename = "mp4alstmp.wav"
            FileUtils.rm_rf(mp4_in_filename)
            FileUtils.cp(in_filename, mp4_in_filename)
            command = "wine64 mp4alsRM23.exe #{mp4_in_filename} #{escaped_out_filename} > /dev/null"
        when "tak"
            # 入力文字列に日本語が含まれているとだめなのでローカルにwavをコピー
            tak_in_filename   = "taktmp.wav"
            # .takでないとエラーを出すので拡張子を.takに切り替え
            tak_out_filename  = Pathname(out_filename).sub_ext(".tak").to_s
            # すでにファイルが存在しないとエラーを出すので消しておく
            FileUtils.rm_rf(tak_in_filename)
            FileUtils.rm_rf(tak_out_filename)
            # 入力をコピー
            FileUtils.cp(in_filename, tak_in_filename)
            # 結果の.tak名を希望した出力ファイル名にリネームする処理を仕込む
            command = "wine64 Takc.exe -e #{@compress_option_string} #{tak_in_filename} #{tak_out_filename} > /dev/null && mv #{tak_out_filename} #{escaped_out_filename} > /dev/null"
        when "sla"
            command = "./sla -es #{escaped_in_filename} #{escaped_out_filename} > /dev/null"
        when "naru"
            command = "./naru -e #{@compress_option_string} #{escaped_in_filename} #{escaped_out_filename} > /dev/null"
        else
            raise "Unsupported tool name."
        end
        execute_time = execute_command(command)
        [execute_time, File.size(out_filename)]
    end

    # 展開処理を実行
    def decompress(in_filename, out_filename)
        command = nil
        escaped_in_filename   = "\"#{in_filename}\""
        escaped_out_filename  = "\"#{out_filename}\""
        case @tool_name
        when "flac"
            command = "flac -d -f -s -o #{escaped_out_filename} #{escaped_in_filename}"
        when "wavpack"
            command = "wvunpack #{escaped_in_filename} -q -y -o #{escaped_out_filename}"
        when "tta"
            command = "tta -d #{escaped_in_filename} #{escaped_out_filename} 2> /dev/null"
        when "monkey's audio"
            # .apeにしないと展開に失敗する...
            ape_in_filename = Pathname(in_filename).sub_ext(".ape").to_s
            FileUtils.mv(in_filename, ape_in_filename)
            escaped_in_filename = "\"#{ape_in_filename}\""
            command = "mac #{escaped_in_filename} #{escaped_out_filename} -d > /dev/null"
        when "optimfrog"
            command = "ofr --decode #{escaped_in_filename} --output #{escaped_out_filename} > /dev/null 2>&1"
        when "mp4als"
            command = "wine64 mp4alsRM23.exe -x #{escaped_in_filename} #{escaped_out_filename} > /dev/null"
        when "tak"
            # .takでないとエラーを出すので拡張子を.takに切り替え
            tak_in_filename  = Pathname(in_filename).sub_ext(".tak").to_s
            command = "wine64 Takc.exe -d #{tak_in_filename} #{escaped_out_filename} > /dev/null"
        when "sla"
            command = "./sla -ds #{escaped_in_filename} #{escaped_out_filename} > /dev/null"
        when "naru"
            command = "./naru -d #{escaped_in_filename} #{escaped_out_filename} > /dev/null"
        else
            raise "Unsupported tool name."
        end
        execute_command(command)
    end

end

# wavの長さ(秒)を取得
def get_wav_length_ms(wavfile_name)
    # データチャンクを取得
    format = nil
    dataChunk = nil
    File.open(wavfile_name) do |file|
        format, chunks = WavFile::readAll(file)
        chunks.each do |c|
            dataChunk = c if c.name == 'data'
        end
    end
    # サンプル数
    numSamples = dataChunk.size / ((format.bitPerSample / 8) * format.channel)
    # ミリ秒に換算
    (1000.0 * numSamples) / format.hz
end

# 結果ファイル名
COMPRESS_SIZE_RESULT_FILE   = "compress_size_result.csv"
COMPRESS_TIME_RESULT_FILE   = "compress_time_result.csv"
DECOMPRESS_TIME_RESULT_FILE = "decompress_time_result.csv"

# 一時ファイル名
COMPRESS_TMP_FILENAME       = "compressed.tmp"
DECOMPRESS_TMP_FILENAME     = "decompressd.tmp"

# テスト対照のツールとその圧縮設定リスト
TEST_TOOL_CONFIGURES = [
    { tool_name: "flac",           compress_option: "-8"},
    { tool_name: "wavpack",        compress_option: "-hh"},
    { tool_name: "tta",            compress_option: ""}, # fmtチャンクによっては読めない...
    #{ tool_name: "mp4als",         compress_option: ""},
    #{ tool_name: "tak",            compress_option: "-p4m"},
    { tool_name: "monkey's audio", compress_option: "-c4000"}, # たまにSEGVで死ぬ...
    # { tool_name: "optimfrog",      compress_option: "--preset max"}, # 出力ファイル名に.ofrが付く...
    # { tool_name: "sla",            compress_option: ""},
    { tool_name: "naru",            compress_option: "-m 4"},
]

# テスト対象の波形が入ったディレクトリリスト
TEST_FILE_DIR_LIST = [
    "./data/**/",
]

# 一時ファイルを削除
FileUtils.rm_rf(COMPRESS_TMP_FILENAME)
FileUtils.rm_rf(DECOMPRESS_TMP_FILENAME)

# 圧縮対象のパスをソートしたリスト作成
filelist = []
TEST_FILE_DIR_LIST.each do |dir|
    filelist.append(Dir.glob(File.join(dir, "*.wav")).sort)
end
filelist = filelist.flatten.sort

# 計測
result_hash = {}
TEST_TOOL_CONFIGURES.each do |config|
    comp = Compressor.new(config[:tool_name], config[:compress_option])
    one_results = {}
    filelist.each do |filename|
        # 圧縮/展開
        puts "[#{config[:tool_name]}] #{filename}"
        comp_time, comp_size = comp.compress(filename, COMPRESS_TMP_FILENAME)
        decomp_time = comp.decompress(COMPRESS_TMP_FILENAME, DECOMPRESS_TMP_FILENAME)
        # 結果をハッシュにぶち込む
        one_results[filename] = { compress_time: comp_time, decompress_time: decomp_time, compressed_size: comp_size }
        # 一時ファイルを削除
        FileUtils.rm_rf(COMPRESS_TMP_FILENAME)
        FileUtils.rm_rf(DECOMPRESS_TMP_FILENAME)
    end
    result_hash[comp.get_compress_configre_string] = one_results
end

# 圧縮率
CSV.open(COMPRESS_SIZE_RESULT_FILE, "w") do |csv|
    # ヘッダ行
    header_line = Array.new
    header_line << "Wave File Name \\ Compress Method"
    header_line << "Original Size"
    result_hash.each do |key, val|
        header_line << key
    end
    csv << header_line

    # 結果行
    filelist.each do |filename|
        result_line   = Array.new
        original_size = File.size(filename)
        result_line << File.basename(filename)
        result_line << original_size
        result_hash.each do |key, val|
            result_line << (val[filename][:compressed_size].to_f * 100) / original_size
        end
        csv << result_line
    end
end

# デコード時間
CSV.open(DECOMPRESS_TIME_RESULT_FILE, "w") do |csv|
    # ヘッダ行
    header_line = Array.new
    header_line << "Wave File Name \\ Compress Method"
    header_line << "Wave Length[ms]"
    result_hash.each do |key, val|
        header_line << key
    end
    csv << header_line

    # 結果行
    filelist.each do |filename|
        result_line   = Array.new
        original_time = get_wav_length_ms(filename)
        result_line << File.basename(filename)
        result_line << original_time
        result_hash.each do |key, val|
            result_line << (val[filename][:decompress_time].to_f * 100) / original_time
        end
        csv << result_line
    end
end
