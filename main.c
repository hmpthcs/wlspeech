#include <stdbool.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <deepspeech.h>
#include <wayland-client.h>
#include "input-method-unstable-v2.h"

// TODO: make configurable
static const char model_path[] = "/usr/share/mozilla/deepspeech/models/ds-model.pbmm";
static const char scorer_path[] = "/usr/share/mozilla/deepspeech/models/ds-model.scorer";
static const char pcm_name[] = "default";

struct ime_state {
	bool active;
	uint32_t serial;
};

struct ime {
	bool running;
	struct zwp_input_method_v2 *wl;
	ModelState *model;
	snd_pcm_t *pcm_capture;
	struct ime_state pending, current;
};

static struct zwp_input_method_manager_v2 *ime_manager = NULL;
static struct wl_seat *seat = NULL;

static void ime_capture(struct ime *ime) {
	int ret = snd_pcm_prepare(ime->pcm_capture);
	if (ret != 0) {
		fprintf(stderr, "snd_pcm_prepare failed\n");
		return;
	}

	StreamingState *stream;
	ret = DS_CreateStream(ime->model, &stream);
	if (ret != 0) {
		fprintf(stderr, "DS_CreateStream failed\n");
		return;
	}

	int duration_sec = 2;
	printf("Recording for %d seconds...\n", duration_sec);

	unsigned int sample_rate = DS_GetModelSampleRate(ime->model);
	size_t buffer_len = duration_sec * sample_rate;
	int16_t *buffer = calloc(1, buffer_len * sizeof(buffer[0]));

	int n = snd_pcm_readi(ime->pcm_capture, buffer, buffer_len);
	if (n < 0) {
		fprintf(stderr, "snd_pcm_readi failed: %s\n", snd_strerror(n));
		return;
	}

	snd_pcm_drop(ime->pcm_capture);

	printf("Recognizing...\n");

	DS_FeedAudioContent(stream, buffer, buffer_len);

	free(buffer);

	char *text = DS_FinishStream(stream);
	printf("Result: %s\n", text);

	zwp_input_method_v2_commit_string(ime->wl, text);
	zwp_input_method_v2_commit(ime->wl, ime->current.serial);

	DS_FreeString(text);
}

static void ime_handle_activate(void *data, struct zwp_input_method_v2 *zwp_input_method_v2) {
	struct ime *ime = data;
	ime->pending.active = true;
}

static void ime_handle_deactivate(void *data, struct zwp_input_method_v2 *zwp_input_method_v2) {
	struct ime *ime = data;
	ime->pending.active = false;
}

static void ime_handle_surrounding_text(void *data, struct zwp_input_method_v2 *zwp_input_method_v2, const char *text, uint32_t cursor, uint32_t anchor) {
	// no-op
}

static void ime_handle_text_change_cause(void *data, struct zwp_input_method_v2 *zwp_input_method_v2, uint32_t cause) {
	// no-op
}

static void ime_handle_content_type(void *data, struct zwp_input_method_v2 *zwp_input_method_v2, uint32_t hint, uint32_t purpose) {
	// TODO
}

static void ime_handle_done(void *data, struct zwp_input_method_v2 *zwp_input_method_v2) {
	struct ime *ime = data;
	ime->pending.serial++;

	if (ime->pending.active && !ime->current.active) {
		ime_capture(ime);
	}

	ime->current = ime->pending;
}

static void ime_handle_unavailable(void *data, struct zwp_input_method_v2 *zwp_input_method_v2) {
	struct ime *ime = data;
	ime->running = false;
	printf("IME unavailable\n");
}

static const struct zwp_input_method_v2_listener ime_listener = {
	.activate = ime_handle_activate,
	.deactivate = ime_handle_deactivate,
	.surrounding_text = ime_handle_surrounding_text,
	.text_change_cause = ime_handle_text_change_cause,
	.content_type = ime_handle_content_type,
	.done = ime_handle_done,
	.unavailable = ime_handle_unavailable,
};

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *iface, uint32_t version) {
	if (strcmp(iface, zwp_input_method_manager_v2_interface.name) == 0) {
		ime_manager = wl_registry_bind(registry, name, &zwp_input_method_manager_v2_interface, 1);
	} else if (seat == NULL && strcmp(iface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	// no-op
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

int main(int argc, char *argv[]) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "wl_display_connect failed\n");
		return 1;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	wl_registry_destroy(registry);

	if (seat == NULL) {
		fprintf(stderr, "No seat found\n");
		return 1;
	}
	if (ime_manager == NULL) {
		fprintf(stderr, "Compositor doesn't support input-method-unstable-v2\n");
		return 1;
	}

	struct ime ime = {
		.running = true,
	};

	int ret = DS_CreateModel(model_path, &ime.model);
	if (ret != 0) {
		fprintf(stderr, "DS_CreateModel failed\n");
		return 1;
	}

	ret = DS_EnableExternalScorer(ime.model, scorer_path);
	if (ret != 0) {
		fprintf(stderr, "DS_EnableExternalScorer failed\n");
		return 1;
	}

	unsigned int sample_rate = DS_GetModelSampleRate(ime.model);

	ret = snd_pcm_open(&ime.pcm_capture, pcm_name, SND_PCM_STREAM_CAPTURE, 0);
	if (ret != 0) {
		fprintf(stderr, "snd_pcm_open failed\n");
		return 1;
	}

	snd_pcm_hw_params_t *pcm_hw_params;
	snd_pcm_hw_params_malloc(&pcm_hw_params);
	snd_pcm_hw_params_any(ime.pcm_capture, pcm_hw_params);
	snd_pcm_hw_params_set_access(ime.pcm_capture, pcm_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(ime.pcm_capture, pcm_hw_params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_rate_near(ime.pcm_capture, pcm_hw_params, &sample_rate, 0);
	snd_pcm_hw_params_set_channels(ime.pcm_capture, pcm_hw_params, 1);

	ret = snd_pcm_hw_params(ime.pcm_capture, pcm_hw_params);
	if (ret != 0) {
		fprintf(stderr, "snd_pcm_hw_params failed\n");
		return 1;
	}

	snd_pcm_hw_params_free(pcm_hw_params);

	ime.wl = zwp_input_method_manager_v2_get_input_method(ime_manager, seat);
	zwp_input_method_v2_add_listener(ime.wl, &ime_listener, &ime);

	while (ime.running) {
		if (wl_display_dispatch(display) < 0) {
			fprintf(stderr, "wl_display_dispatch failed\n");
			return 1;
		}
	}

	snd_pcm_close(ime.pcm_capture);
	DS_FreeModel(ime.model);
	return 0;
}
