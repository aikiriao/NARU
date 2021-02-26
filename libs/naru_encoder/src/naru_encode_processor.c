#include "naru_encode_processor.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <stdlib.h>
#include <string.h>

struct NARUEncodeProcessor* NARUEncodeProcessor_Create(uint8_t max_filter_order)
{
  struct NARUEncodeProcessor *processor;

  NARU_ASSERT(max_filter_order != 0);
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(max_filter_order));

  processor = (struct NARUEncodeProcessor *)malloc(sizeof(struct NARUEncodeProcessor));

  processor->weight = (int32_t *)malloc(sizeof(int32_t) * max_filter_order);
  processor->ar_coef = (int32_t *)malloc(sizeof(int32_t) * (max_filter_order / 2));
  processor->grad = (int32_t *)malloc(sizeof(int32_t) * max_filter_order);

  return processor;
}

void NARUEncodeProcessor_Destroy(struct NARUEncodeProcessor *processor)
{
  NARU_ASSERT(processor != NULL);

  NARU_NULLCHECK_AND_FREE(processor->weight);
  NARU_NULLCHECK_AND_FREE(processor->ar_coef);
  NARU_NULLCHECK_AND_FREE(processor->grad);
  NARU_NULLCHECK_AND_FREE(processor);
}

void NARUEncodeProcessor_Predict(
    struct NARUEncodeProcessor *processor,
    const int32_t *input, uint32_t num_samples, int32_t *residual)
{
  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(input != NULL);
  NARU_ASSERT(residual != NULL);

  /* TODO: stab 何もせずそのまま出力 */
  memcpy(residual, input, sizeof(int32_t) * num_samples);
}
