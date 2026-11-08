#include <gtest/gtest.h>

#include "VxFPUControlWord.h"
#include "VxVector.h"

VxVector VxTestLibraryNormalize(const VxVector &value);
float VxTestLibraryMagnitude(const VxVector &value);

TEST(ScalarConsumerTest, Ia32CallerUsesLibraryVectorEvaluation) {
    struct RestoreControl {
        XWORD Word = VxGetFPUControlWord();
        ~RestoreControl() { VxSetFPUControlWord(Word); }
    } restore;
    VxVector (*volatile referenceNormalize)(const VxVector &) = VxTestLibraryNormalize;
    float (*volatile referenceMagnitude)(const VxVector &) = VxTestLibraryMagnitude;
    const VxVector inputs[] = {VxVector(.9848077297f, .1736481637f, 0), VxVector(-.963630438f, 0, .267238319f),
                               VxVector(.968147635f, 0, .250380158f)};
    const XWORD precisions[] = {0, 0x0200, 0x0300};
    for (XWORD precision : precisions) {
        VxSetFPUControlWord(static_cast<XWORD>((restore.Word & ~0x0f00) | precision | 0x003f));
        for (const VxVector &input : inputs) {
            const VxVector expected = referenceNormalize(input);
            const VxVector actual = Normalize(input);
            for (int component = 0; component < 3; ++component)
                EXPECT_EQ(expected[component], actual[component]);
            EXPECT_EQ(referenceMagnitude(input), Magnitude(input));
        }
    }
}
