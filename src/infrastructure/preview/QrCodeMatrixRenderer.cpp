#include "sleekpr/infrastructure/preview/QrCodeMatrixRenderer.h"

#include <QByteArray>

#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace sleekpr::infrastructure {
namespace {

constexpr int EccFormatBitsQ = 0b11;

struct QrSpec
{
    int version = 1;
    int matrixSize = 21;
    int dataCodewordCount = 13;
    int errorCorrectionCodewordsPerBlock = 13;
    int maxPayloadBytes = 11;
    std::vector<int> alignmentCenters;
    std::vector<int> dataCodewordBlocks;
};

struct QrBlock
{
    std::vector<int> dataCodewords;
    std::vector<int> errorCorrectionCodewords;
};

struct GaloisTables
{
    std::array<int, 512> exp{};
    std::array<int, 256> log{};
};

struct WorkMatrix
{
    explicit WorkMatrix(int size)
        : matrixSize(size)
        , modules(static_cast<size_t>(size * size), false)
        , reserved(static_cast<size_t>(size * size), false)
    {
    }

    int matrixSize = 0;
    std::vector<bool> modules;
    std::vector<bool> reserved;

    bool module(int x, int y) const
    {
        return modules[static_cast<size_t>(y * matrixSize + x)];
    }

    bool isReserved(int x, int y) const
    {
        return reserved[static_cast<size_t>(y * matrixSize + x)];
    }

    void setModule(int x, int y, bool dark)
    {
        modules[static_cast<size_t>(y * matrixSize + x)] = dark;
    }

    void setFunctionModule(int x, int y, bool dark)
    {
        setModule(x, y, dark);
        reserved[static_cast<size_t>(y * matrixSize + x)] = true;
    }
};

QrSpec specForPayloadSize(int payloadSize)
{
    if (payloadSize <= 11) {
        return QrSpec{1, 21, 13, 13, 11, {}, {13}};
    }
    if (payloadSize <= 20) {
        return QrSpec{2, 25, 22, 22, 20, {6, 18}, {22}};
    }
    if (payloadSize <= 32) {
        return QrSpec{3, 29, 34, 18, 32, {6, 22}, {17, 17}};
    }
    if (payloadSize <= 46) {
        return QrSpec{4, 33, 48, 26, 46, {6, 26}, {24, 24}};
    }
    throw std::length_error("QR Version 4-Q 当前最多支持 46 个 UTF-8 字节。");
}

void appendBits(std::vector<bool>& bits, int value, int bitCount)
{
    for (int index = bitCount - 1; index >= 0; --index) {
        bits.push_back(((value >> index) & 1) != 0);
    }
}

std::vector<int> createDataCodewords(const QByteArray& payload, const QrSpec& spec)
{
    std::vector<bool> bits;
    bits.reserve(static_cast<size_t>(spec.dataCodewordCount * 8));

    // QR 字节模式：模式指示符 0100，Version 1-9 的字符计数字段为 8 bit。
    appendBits(bits, 0b0100, 4);
    appendBits(bits, payload.size(), 8);
    for (const auto byte : payload) {
        appendBits(bits, static_cast<unsigned char>(byte), 8);
    }

    const int capacityBits = spec.dataCodewordCount * 8;
    const int terminatorBits = std::min(4, capacityBits - static_cast<int>(bits.size()));
    appendBits(bits, 0, terminatorBits);
    while ((bits.size() % 8) != 0) {
        bits.push_back(false);
    }

    std::vector<int> codewords;
    codewords.reserve(static_cast<size_t>(spec.dataCodewordCount));
    for (size_t bitIndex = 0; bitIndex < bits.size(); bitIndex += 8) {
        int value = 0;
        for (int offset = 0; offset < 8; ++offset) {
            value = (value << 1) | (bits[bitIndex + offset] ? 1 : 0);
        }
        codewords.push_back(value);
    }

    // QR 标准要求用 0xEC/0x11 交替补齐数据码字，保证短编号也能形成完整码流。
    for (int padIndex = 0; static_cast<int>(codewords.size()) < spec.dataCodewordCount; ++padIndex) {
        codewords.push_back((padIndex % 2 == 0) ? 0xEC : 0x11);
    }

    return codewords;
}

GaloisTables createGaloisTables()
{
    GaloisTables tables;
    int value = 1;
    for (int index = 0; index < 255; ++index) {
        tables.exp[index] = value;
        tables.log[value] = index;
        value <<= 1;
        if ((value & 0x100) != 0) {
            value ^= 0x11D;
        }
    }
    for (int index = 255; index < 512; ++index) {
        tables.exp[index] = tables.exp[index - 255];
    }
    return tables;
}

int gfMultiply(const GaloisTables& tables, int left, int right)
{
    if (left == 0 || right == 0) {
        return 0;
    }
    return tables.exp[tables.log[left] + tables.log[right]];
}

std::vector<int> createGeneratorPolynomial(const GaloisTables& tables, int degree)
{
    std::vector<int> polynomial{1};
    for (int index = 0; index < degree; ++index) {
        std::vector<int> next(polynomial.size() + 1, 0);
        for (size_t coefficientIndex = 0; coefficientIndex < polynomial.size(); ++coefficientIndex) {
            next[coefficientIndex] ^= polynomial[coefficientIndex];
            next[coefficientIndex + 1] ^= gfMultiply(tables, polynomial[coefficientIndex], tables.exp[index]);
        }
        polynomial = std::move(next);
    }
    return polynomial;
}

std::vector<int> createErrorCorrectionCodewords(const std::vector<int>& dataCodewords, int errorCorrectionCodewordCount)
{
    const auto tables = createGaloisTables();
    const auto generator = createGeneratorPolynomial(tables, errorCorrectionCodewordCount);
    std::vector<int> remainder(static_cast<size_t>(errorCorrectionCodewordCount), 0);

    // Reed-Solomon 长除法：数据码字逐个进入寄存器，最终寄存器内容就是纠错码字。
    for (const auto codeword : dataCodewords) {
        const int factor = codeword ^ remainder.front();
        for (int index = 0; index < errorCorrectionCodewordCount - 1; ++index) {
            remainder[index] = remainder[index + 1];
        }
        remainder.back() = 0;
        for (int index = 0; index < errorCorrectionCodewordCount; ++index) {
            remainder[index] ^= gfMultiply(tables, generator[index + 1], factor);
        }
    }

    return remainder;
}

std::vector<QrBlock> createBlocks(const std::vector<int>& dataCodewords, const QrSpec& spec)
{
    std::vector<QrBlock> blocks;
    blocks.reserve(spec.dataCodewordBlocks.size());

    size_t cursor = 0;
    for (const auto blockDataCodewordCount : spec.dataCodewordBlocks) {
        const auto blockStart = dataCodewords.cbegin() + static_cast<std::ptrdiff_t>(cursor);
        const auto blockEnd = blockStart + blockDataCodewordCount;
        QrBlock block;
        block.dataCodewords = std::vector<int>(blockStart, blockEnd);
        block.errorCorrectionCodewords = createErrorCorrectionCodewords(block.dataCodewords, spec.errorCorrectionCodewordsPerBlock);
        blocks.push_back(std::move(block));
        cursor += static_cast<size_t>(blockDataCodewordCount);
    }

    return blocks;
}

std::vector<int> interleaveCodewords(const std::vector<QrBlock>& blocks, const QrSpec& spec)
{
    std::vector<int> codewords;
    codewords.reserve(static_cast<size_t>(spec.dataCodewordCount + spec.errorCorrectionCodewordsPerBlock * blocks.size()));

    int maxDataBlockSize = 0;
    for (const auto& block : blocks) {
        maxDataBlockSize = std::max(maxDataBlockSize, static_cast<int>(block.dataCodewords.size()));
    }

    // 多块 QR 需要按列交织数据码字和纠错码字；Version 1/2 只有一个块，仍会得到原顺序。
    for (int index = 0; index < maxDataBlockSize; ++index) {
        for (const auto& block : blocks) {
            if (index < static_cast<int>(block.dataCodewords.size())) {
                codewords.push_back(block.dataCodewords[static_cast<size_t>(index)]);
            }
        }
    }
    for (int index = 0; index < spec.errorCorrectionCodewordsPerBlock; ++index) {
        for (const auto& block : blocks) {
            codewords.push_back(block.errorCorrectionCodewords[static_cast<size_t>(index)]);
        }
    }

    return codewords;
}

std::vector<bool> codewordsToBits(const std::vector<int>& codewords)
{
    std::vector<bool> bits;
    bits.reserve(codewords.size() * 8);
    for (const auto codeword : codewords) {
        appendBits(bits, codeword, 8);
    }
    return bits;
}

void drawFinderPattern(WorkMatrix& matrix, int left, int top)
{
    for (int y = -1; y <= 7; ++y) {
        for (int x = -1; x <= 7; ++x) {
            const int moduleX = left + x;
            const int moduleY = top + y;
            if (moduleX < 0 || moduleY < 0 || moduleX >= matrix.matrixSize || moduleY >= matrix.matrixSize) {
                continue;
            }

            // 探测图形外围保留一圈白色分隔符，中心 3x3 和外框为黑色。
            const bool separator = x == -1 || x == 7 || y == -1 || y == 7;
            const bool dark = !separator
                && (x == 0 || x == 6 || y == 0 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4));
            matrix.setFunctionModule(moduleX, moduleY, dark);
        }
    }
}

void reserveFormatModules(WorkMatrix& matrix)
{
    for (int index = 0; index <= 5; ++index) {
        matrix.setFunctionModule(8, index, false);
        matrix.setFunctionModule(index, 8, false);
    }
    matrix.setFunctionModule(8, 7, false);
    matrix.setFunctionModule(8, 8, false);
    matrix.setFunctionModule(7, 8, false);
    for (int index = 9; index < 15; ++index) {
        matrix.setFunctionModule(14 - index, 8, false);
    }
    for (int index = 0; index < 8; ++index) {
        matrix.setFunctionModule(matrix.matrixSize - 1 - index, 8, false);
    }
    for (int index = 8; index < 15; ++index) {
        matrix.setFunctionModule(8, matrix.matrixSize - 15 + index, false);
    }
}

bool overlapsFinderPattern(int centerX, int centerY, int matrixSize)
{
    return (centerX <= 8 && centerY <= 8)
        || (centerX >= matrixSize - 9 && centerY <= 8)
        || (centerX <= 8 && centerY >= matrixSize - 9);
}

void drawAlignmentPattern(WorkMatrix& matrix, int centerX, int centerY)
{
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            const bool dark = std::max(std::abs(x), std::abs(y)) != 1;
            matrix.setFunctionModule(centerX + x, centerY + y, dark);
        }
    }
}

void drawFunctionPatterns(WorkMatrix& matrix, const QrSpec& spec)
{
    drawFinderPattern(matrix, 0, 0);
    drawFinderPattern(matrix, matrix.matrixSize - 7, 0);
    drawFinderPattern(matrix, 0, matrix.matrixSize - 7);

    // Version 1-4 的横纵定时线规则一致；Version 2 及以上按规格补校正图形。
    for (int index = 8; index < matrix.matrixSize - 8; ++index) {
        const bool dark = (index % 2) == 0;
        matrix.setFunctionModule(6, index, dark);
        matrix.setFunctionModule(index, 6, dark);
    }
    for (const auto centerY : spec.alignmentCenters) {
        for (const auto centerX : spec.alignmentCenters) {
            if (!overlapsFinderPattern(centerX, centerY, matrix.matrixSize)) {
                drawAlignmentPattern(matrix, centerX, centerY);
            }
        }
    }
    matrix.setFunctionModule(8, matrix.matrixSize - 8, true);
    reserveFormatModules(matrix);
}

bool maskBit(int mask, int x, int y)
{
    switch (mask) {
    case 0:
        return ((x + y) % 2) == 0;
    case 1:
        return (y % 2) == 0;
    case 2:
        return (x % 3) == 0;
    case 3:
        return ((x + y) % 3) == 0;
    case 4:
        return (((y / 2) + (x / 3)) % 2) == 0;
    case 5:
        return (((x * y) % 2) + ((x * y) % 3)) == 0;
    case 6:
        return ((((x * y) % 2) + ((x * y) % 3)) % 2) == 0;
    case 7:
        return ((((x + y) % 2) + ((x * y) % 3)) % 2) == 0;
    default:
            return false;
    }
}

void drawDataModules(WorkMatrix& matrix, const std::vector<bool>& payloadBits, int mask)
{
    size_t bitIndex = 0;
    bool upward = true;

    // QR 数据从右下角开始按双列蛇形落点，跳过定时线所在的第 6 列。
    for (int right = matrix.matrixSize - 1; right >= 1; right -= 2) {
        if (right == 6) {
            --right;
        }

        for (int verticalIndex = 0; verticalIndex < matrix.matrixSize; ++verticalIndex) {
            const int y = upward ? matrix.matrixSize - 1 - verticalIndex : verticalIndex;
            for (int columnOffset = 0; columnOffset < 2; ++columnOffset) {
                const int x = right - columnOffset;
                if (matrix.isReserved(x, y)) {
                    continue;
                }

                bool dark = bitIndex < payloadBits.size() ? payloadBits[bitIndex] : false;
                ++bitIndex;
                if (maskBit(mask, x, y)) {
                    dark = !dark;
                }
                matrix.setModule(x, y, dark);
            }
        }

        upward = !upward;
    }
}

int createFormatBits(int mask)
{
    const int data = (EccFormatBitsQ << 3) | mask;
    int remainder = data;
    for (int index = 0; index < 10; ++index) {
        remainder = (remainder << 1) ^ (((remainder >> 9) & 1) != 0 ? 0x537 : 0);
    }
    return ((data << 10) | (remainder & 0x3FF)) ^ 0x5412;
}

bool formatBit(int formatBits, int index)
{
    return ((formatBits >> index) & 1) != 0;
}

void drawFormatBits(WorkMatrix& matrix, int mask)
{
    const int bits = createFormatBits(mask);
    for (int index = 0; index <= 5; ++index) {
        matrix.setFunctionModule(8, index, formatBit(bits, index));
        matrix.setFunctionModule(index, 8, formatBit(bits, index));
    }
    matrix.setFunctionModule(8, 7, formatBit(bits, 6));
    matrix.setFunctionModule(8, 8, formatBit(bits, 7));
    matrix.setFunctionModule(7, 8, formatBit(bits, 8));
    for (int index = 9; index < 15; ++index) {
        matrix.setFunctionModule(14 - index, 8, formatBit(bits, index));
    }
    for (int index = 0; index < 8; ++index) {
        matrix.setFunctionModule(matrix.matrixSize - 1 - index, 8, formatBit(bits, index));
    }
    for (int index = 8; index < 15; ++index) {
        matrix.setFunctionModule(8, matrix.matrixSize - 15 + index, formatBit(bits, index));
    }
    matrix.setFunctionModule(8, matrix.matrixSize - 8, true);
}

int runPenalty(int runLength)
{
    return runLength >= 5 ? 3 + (runLength - 5) : 0;
}

int evaluateLinePenalty(const WorkMatrix& matrix, bool horizontal, int index)
{
    int penalty = 0;
    bool previous = horizontal ? matrix.module(0, index) : matrix.module(index, 0);
    int runLength = 1;
    for (int cursor = 1; cursor < matrix.matrixSize; ++cursor) {
        const bool current = horizontal ? matrix.module(cursor, index) : matrix.module(index, cursor);
        if (current == previous) {
            ++runLength;
            continue;
        }
        penalty += runPenalty(runLength);
        previous = current;
        runLength = 1;
    }
    penalty += runPenalty(runLength);
    return penalty;
}

int evaluateMaskPenalty(const WorkMatrix& matrix)
{
    int penalty = 0;
    int darkCount = 0;
    for (int index = 0; index < matrix.matrixSize; ++index) {
        penalty += evaluateLinePenalty(matrix, true, index);
        penalty += evaluateLinePenalty(matrix, false, index);
    }

    for (int y = 0; y < matrix.matrixSize; ++y) {
        for (int x = 0; x < matrix.matrixSize; ++x) {
            if (matrix.module(x, y)) {
                ++darkCount;
            }
            if (x + 1 < matrix.matrixSize && y + 1 < matrix.matrixSize) {
                const bool color = matrix.module(x, y);
                if (matrix.module(x + 1, y) == color
                    && matrix.module(x, y + 1) == color
                    && matrix.module(x + 1, y + 1) == color) {
                    penalty += 3;
                }
            }
        }
    }

    const int percent = (darkCount * 100) / (matrix.matrixSize * matrix.matrixSize);
    penalty += (std::abs(percent - 50) / 5) * 10;
    return penalty;
}

QrCodeMatrix buildBestMatrix(const std::vector<bool>& payloadBits, const QrSpec& spec)
{
    int bestPenalty = std::numeric_limits<int>::max();
    QrCodeMatrix bestMatrix;

    // 八种 QR 掩膜都有效；选择罚分最低的掩膜，减少大面积同色块和扫码失败概率。
    for (int mask = 0; mask < 8; ++mask) {
        WorkMatrix matrix(spec.matrixSize);
        drawFunctionPatterns(matrix, spec);
        drawDataModules(matrix, payloadBits, mask);
        drawFormatBits(matrix, mask);

        const int penalty = evaluateMaskPenalty(matrix);
        if (penalty < bestPenalty) {
            bestPenalty = penalty;
            bestMatrix = QrCodeMatrix(spec.matrixSize, std::move(matrix.modules));
        }
    }

    return bestMatrix;
}

} // 匿名命名空间

QrCodeMatrix QrCodeMatrixRenderer::render(const QString& payload) const
{
    const auto trimmed = payload.trimmed();
    if (trimmed.isEmpty()) {
        throw std::invalid_argument("QR 内容不能为空。");
    }

    const auto payloadBytes = trimmed.toUtf8();
    const auto spec = specForPayloadSize(payloadBytes.size());
    const auto dataCodewords = createDataCodewords(payloadBytes, spec);
    const auto blocks = createBlocks(dataCodewords, spec);
    return buildBestMatrix(codewordsToBits(interleaveCodewords(blocks, spec)), spec);
}

} // 命名空间 sleekpr::infrastructure
