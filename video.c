
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>

#ifndef guess_stream_format
# define guess_stream_format av_guess_format
#endif  /* guess_stream_format */

#ifndef CODEC_TYPE_VIDEO
# define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#endif  /* CODEC_TYPE_VIDEO */

#ifndef CODEC_TYPE_SUBTITLE
# define CODEC_TYPE_SUBTITLE AVMEDIA_TYPE_SUBTITLE
#endif  /* CODEC_TYPE_SUBTITLE */

#ifndef PKT_FLAG_KEY
# define PKT_FLAG_KEY AV_PKT_FLAG_KEY
#endif  /* PKT_FLAG_KEY */

#include "automaton.h"
#include "video.h"


#define CELL_SIZE  10
#define FRAME_RATE 25  /* subtitle stream does not support FPS < 20 */

#define OUTBUF_SIZE 200000
static uint8_t* video_outbuf;
static AVFrame *picture, *tmp_picture;

static AVFrame* alloc_picture(enum PixelFormat pix_fmt, int width, int height)
{
	uint8_t *picture_buf;
	int size;

	picture = avcodec_alloc_frame();
	if(!picture)
		return NULL;
	size = avpicture_get_size(pix_fmt, width, height);
	picture_buf = av_malloc(size);
	if(!picture_buf)
	{
		av_free(picture);
		return NULL;
	}
	avpicture_fill((AVPicture *)picture, picture_buf,
		pix_fmt, width, height);
	return picture;
}

typedef struct
{
	AVFormatContext* oc;
	AVStream* st;
	AVStream* st_sub;
	int frame_count;
} Renderer;

void* open_renderer(struct Automaton* automaton, const char* filename)
{
	Renderer* res;
	AVOutputFormat* fmt;
	AVFormatContext* oc;
	AVCodecContext *c = NULL, *c_sub = NULL;
	AVStream *st = NULL, *st_sub = NULL;
	AVCodec *codec, *codec_sub;
	int height = automaton->height * CELL_SIZE;
	int width  = automaton->width  * CELL_SIZE;
	if(height % 2 != 0) height++; /* Размеры изображения должны быть */
	if(width  % 2 != 0) width ++; /*  степенями двойки               */

	av_register_all();

	/* Автоматически определить выходной формат по имени файла,
	по умолчанию avi. */
	fmt = guess_stream_format(NULL, filename, NULL);
	if(!fmt)
	{
		printf("Couldn't deduce output format from file extension: using AVI.\n");
		fmt = guess_stream_format("avi", NULL, NULL);
	}
	if(!fmt)
	{
		printf("Could not find suitable output format\n");
		return NULL;
	}

	/***********************/
	/* Чёрная магия ffmpeg */
	/***********************/
	oc = avformat_alloc_context();
	if(!oc)
	{
		printf("Memory error\n");
		return NULL;
	}
	oc->oformat = fmt;
	snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

	if(fmt->video_codec != CODEC_ID_NONE)
	{
		/* Добавить видеопоток */
		st = av_new_stream(oc, 0);
		if(!st)
		{
			printf("Could not alloc stream\n");
			return NULL;
		}
		c = st->codec;
		c->codec_id = fmt->video_codec;
		c->codec_type = CODEC_TYPE_VIDEO;
		c->bit_rate = 400000;
		c->width = width;
		c->height = height;
		c->time_base.den = FRAME_RATE;
		c->time_base.num = 1;
		c->gop_size = 12;
		c->pix_fmt = PIX_FMT_YUV420P;
		if(oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	/* Добавить субтитры */
	st_sub = av_new_stream(oc, 0);
	if(!st_sub)
	{
		printf("Could not alloc stream\n");
		return NULL;
	}
	c_sub = st_sub->codec;
	c_sub->codec_id = CODEC_ID_SRT;
	c_sub->codec_type = CODEC_TYPE_SUBTITLE;
	c_sub->time_base.den = FRAME_RATE;
	c_sub->time_base.num = 1;
	c_sub->width = width;
	c_sub->height = height;
	if(oc->oformat->flags & AVFMT_GLOBALHEADER)
		c_sub->flags |= CODEC_FLAG_GLOBAL_HEADER;
	if(ff_ass_subtitle_header_default(c_sub) < 0)
	{
		printf("Could not add subtitles!\n");
		return NULL;
	}

	if(av_set_parameters(oc, NULL) < 0)
	{
		printf("Invalid output format parameters\n");
		return NULL;
	}
	dump_format(oc, 0, filename, 1);

	codec = avcodec_find_encoder(c->codec_id);
	if(!codec)
	{
		printf("Codec not found\n");
		return NULL;
	}
	if(avcodec_open(c, codec) < 0)
	{
		printf("Could not open codec\n");
		return NULL;
	}

	codec_sub = avcodec_find_encoder(c_sub->codec_id);
	if(!codec_sub)
	{
		printf("Subtitles codec not found\n");
		return NULL;
	}
	if(avcodec_open(c_sub, codec_sub) < 0)
	{
		printf("Could not open subtitles codec\n");
		return NULL;
	}

	video_outbuf = NULL;
	if(!(oc->oformat->flags & AVFMT_RAWPICTURE))
		video_outbuf = av_malloc(OUTBUF_SIZE);
	picture = alloc_picture(c->pix_fmt, c->width, c->height);
	if(!picture)
	{
		printf("Could not allocate picture\n");
		return NULL;
	}
	tmp_picture = NULL;
	if(c->pix_fmt != PIX_FMT_YUV420P)
	{
		tmp_picture = alloc_picture(PIX_FMT_YUV420P, c->width, c->height);
		if(!tmp_picture)
		{
			printf("Could not allocate temporary picture\n");
			return NULL;
		}
	}
	if(url_fopen(&oc->pb, filename, URL_WRONLY) < 0)
	{
		printf("Could not open '%s'\n", filename);
		return NULL;
	}

	if(av_write_header(oc) != 0)
	{
		printf("Unable to write header\n");
		return NULL;
	}

	res = (Renderer*)malloc(sizeof(Renderer));
	res->oc = oc;
	res->st = st;
	res->st_sub = st_sub;
	res->frame_count = 0;
	return res;
}

static const int palette[14] = { 0, 0, -70, -100, 20, -80, 140, 180,
	-80, 20, 80, -60, -140, 80};

static void fill_yuv_image(AVFrame* pict, struct Automaton* automaton)
{
	int x, y, k,
		n = automaton->nstates < 7 ? automaton->nstates : 7;

	/* Y */
	for(y = 0; y < automaton->height * CELL_SIZE; y++)
		for(x = 0; x < automaton->width * CELL_SIZE; x++)
			pict->data[0][y * pict->linesize[0] + x] = 120;

	/* Cb & Cr */
	for(y = 0; y < CELL_SIZE * automaton->height / 2; y++)
		for(x = 0; x < CELL_SIZE * automaton->width / 2; x++)
		{
			int U = -50, V = 100; /* нулевое состояние */
			int i = 2 * y / CELL_SIZE,
				j = 2 * x / CELL_SIZE;

			for(k = 0; k < n; k++)
			{
				if(CELL(automaton, j, i) == automaton->states[k])
				{
					U = palette[2*k    ];
					V = palette[2*k + 1];
					break;
				}
			}

			pict->data[1][y * pict->linesize[1] + x] = U;
			pict->data[2][y * pict->linesize[2] + x] = V;
		}
}

#define SUB_BUFFER_SIZE 16536
static unsigned char sub_buffer[SUB_BUFFER_SIZE];

#define SUB_STR_LEN 1024
static char sub_str[SUB_STR_LEN];

void render(void* renderer, struct Automaton* automaton, const char* text)
{
	Renderer* r = renderer;
	AVFormatContext* oc = r->oc;
	AVStream* st = r->st;
	int out_size, ret;
	AVCodecContext *c = st->codec;
	static struct SwsContext* img_convert_ctx;
	AVSubtitle sub;

	if(c->pix_fmt != PIX_FMT_YUV420P)
	{
		if(img_convert_ctx == NULL)
		{
			img_convert_ctx = sws_getContext(c->width, c->height,
				PIX_FMT_YUV420P,
				c->width, c->height,
				c->pix_fmt,
				SWS_FAST_BILINEAR, NULL, NULL, NULL);
			if (img_convert_ctx == NULL)
			{
				printf("Cannot initialize the conversion context\n");
				return;
			}
		}
		fill_yuv_image(tmp_picture, automaton);
		sws_scale(img_convert_ctx, (const uint8_t* const*)tmp_picture->data,
			tmp_picture->linesize, 0, c->height, picture->data, picture->linesize);
	} else {
		fill_yuv_image(picture, automaton);
	}

	out_size = avcodec_encode_video(c, video_outbuf, OUTBUF_SIZE, picture);
	if(out_size > 0)
	{
		AVPacket pkt;
		av_init_packet(&pkt);
		if(c->coded_frame->pts != (int64_t)AV_NOPTS_VALUE)
			pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base,
				st->time_base);
		if(c->coded_frame->key_frame)
			pkt.flags |= PKT_FLAG_KEY;
		pkt.stream_index= st->index;
		pkt.data= video_outbuf;
		pkt.size= out_size;
		ret = av_interleaved_write_frame(oc, &pkt);
	} else {
		ret = 0;
	}

	if(ret != 0)
	{
		printf("Error writing video frame\n");
		return;
	}

	snprintf(sub_str, SUB_STR_LEN, "Frame %d", r->frame_count);
	memset(&sub, 0, sizeof(sub));
	if(text)
	{
		strcat(sub_str, "  ");
		strncat(sub_str, text, SUB_STR_LEN - strlen(sub_str));
	}
	if(ff_ass_add_rect(&sub, sub_str, 100 * r->frame_count / FRAME_RATE,
		4 + 100 * r->frame_count / FRAME_RATE, 0) <= 0)
	{
		printf("Unable to render subtitle\n");
	} else
	{
		ret = avcodec_encode_subtitle(r->st_sub->codec, &sub_buffer[0],
			SUB_BUFFER_SIZE, &sub);
		if(ret < 0)
		{
			printf("Unable to encode subtitle\n");
		} else {
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.stream_index = r->st_sub->index;
			if(c->coded_frame->pts != (int64_t)AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base,
					st->time_base);
			if(c->coded_frame->key_frame)
				pkt.flags |= PKT_FLAG_KEY;
			pkt.data = sub_buffer;
			pkt.size = ret;
			if(av_interleaved_write_frame(oc, &pkt) < 0)
			{
				printf("Unable to write subtitle frame\n");
			}
		}
	}

	r->frame_count++;
}

void close_renderer(void* renderer)
{
	unsigned int i;
	Renderer* r = renderer;
	AVFormatContext* oc = r->oc;
	AVStream* st = r->st;

	av_write_trailer(oc);

	avcodec_close(st->codec);
	av_free(picture->data[0]);
	av_free(picture);
	if(tmp_picture)
	{
		av_free(tmp_picture->data[0]);
		av_free(tmp_picture);
	}
	av_free(video_outbuf);
	for(i = 0; i < oc->nb_streams; i++)
	{
		av_freep(&oc->streams[i]->codec);
		av_freep(&oc->streams[i]);
	}
	url_fclose(oc->pb);
	av_free(oc);
}

