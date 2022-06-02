#include <stdio.h>
#include <alsa/asoundlib.h>
#include <deepspeech.h>

// TODO: make configurable
static const char model_path[] = "/usr/share/mozilla/deepspeech/models/ds-model.pbmm";
static const char scorer_path[] = "/usr/share/mozilla/deepspeech/models/ds-model.scorer";
static const char pcm_name[] = "default";

int main(int argc, char *argv[]) {
	ModelState *model;
	int ret = DS_CreateModel(model_path, &model);
	if (ret != 0) {
		fprintf(stderr, "DS_CreateModel failed\n");
		return 1;
	}

	unsigned int sample_rate = DS_GetModelSampleRate(model);
	printf("Model sample rate: %d\n", sample_rate);

	ret = DS_EnableExternalScorer(model, scorer_path);
	if (ret != 0) {
		fprintf(stderr, "DS_EnableExternalScorer failed\n");
		return 1;
	}

	snd_pcm_t *pcm_capture;
	ret = snd_pcm_open(&pcm_capture, pcm_name, SND_PCM_STREAM_CAPTURE, 0);
	if (ret != 0) {
		fprintf(stderr, "snd_pcm_open failed\n");
		return 1;
	}

	snd_pcm_hw_params_t *pcm_hw_params;
	snd_pcm_hw_params_malloc(&pcm_hw_params);
	snd_pcm_hw_params_any(pcm_capture, pcm_hw_params);
	snd_pcm_hw_params_set_access(pcm_capture, pcm_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(pcm_capture, pcm_hw_params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_rate_near(pcm_capture, pcm_hw_params, &sample_rate, 0);
	snd_pcm_hw_params_set_channels(pcm_capture, pcm_hw_params, 1);

	ret = snd_pcm_hw_params(pcm_capture, pcm_hw_params);
	if (ret != 0) {
		fprintf(stderr, "snd_pcm_hw_params failed\n");
		return 1;
	}

	snd_pcm_hw_params_free(pcm_hw_params);

	ret = snd_pcm_prepare(pcm_capture);
	if (ret != 0) {
		fprintf(stderr, "snd_pcm_prepare failed\n");
		return 1;
	}

	StreamingState *stream;
	ret = DS_CreateStream(model, &stream);
	if (ret != 0) {
		fprintf(stderr, "DS_CreateStream failed\n");
		return 1;
	}

	int duration_sec = 2;
	printf("Recording for %d seconds...\n", duration_sec);

	size_t buffer_len = duration_sec * sample_rate;
	int16_t *buffer = calloc(1, buffer_len * sizeof(buffer[0]));

	int n = snd_pcm_readi(pcm_capture, buffer, buffer_len);
	if (n < 0) {
		fprintf(stderr, "snd_pcm_readi failed: %s\n", snd_strerror(n));
		return 1;
	}

	printf("Recognizing...\n");

	DS_FeedAudioContent(stream, buffer, buffer_len);

	free(buffer);

	char *text = DS_FinishStream(stream);
	printf("Result: %s\n", text);
	DS_FreeString(text);

	snd_pcm_close(pcm_capture);

	DS_FreeModel(model);
	return 0;
}
