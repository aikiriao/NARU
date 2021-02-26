#include "naru_decode_processor.h"
#include "naru_internal.h"
#include "naru_utility.h"

#include <stdlib.h>
#include <string.h>

struct NARUDecodeProcessor* NARUDecodeProcessor_Create(uint8_t max_filter_order)
{
  struct NARUDecodeProcessor *processor;

  NARU_ASSERT(max_filter_order != 0);
  NARU_ASSERT(NARUUTILITY_IS_POWERED_OF_2(max_filter_order));

  processor = (struct NARUDecodeProcessor *)malloc(sizeof(struct NARUDecodeProcessor));

  processor->weight = (int32_t *)malloc(sizeof(int32_t) * max_filter_order);
  processor->ar_coef = (int32_t *)malloc(sizeof(int32_t) * (max_filter_order / 2));
  processor->grad = (int32_t *)malloc(sizeof(int32_t) * max_filter_order);

  return processor;
}

void NARUDecodeProcessor_Destroy(struct NARUDecodeProcessor *processor)
{
  NARU_ASSERT(processor != NULL);

  NARU_NULLCHECK_AND_FREE(processor->weight);
  NARU_NULLCHECK_AND_FREE(processor->ar_coef);
  NARU_NULLCHECK_AND_FREE(processor->grad);
  NARU_NULLCHECK_AND_FREE(processor);
}

void NARUDecodeProcessor_Synthesize(
    struct NARUDecodeProcessor *processor,
    const int32_t *residual, uint32_t num_samples, int32_t *output)
{
  NARU_ASSERT(processor != NULL);
  NARU_ASSERT(residual != NULL);
  NARU_ASSERT(output != NULL);

  /* TODO: stab 何もせずそのまま出力 */
  memcpy(output, residual, sizeof(int32_t) * num_samples);
}
