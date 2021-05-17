﻿#include <stdio.h>
#include "mp4_to_flv.h"
#include <SDL.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#undef main

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

void play_video_only(const char* filename)
{

  int ret = -1;

  AVFormatContext *pFormatCtx = NULL; //for opening multi-media file

  int             i, videoStream;

  AVCodecContext  *pCodecCtxOrig = NULL; //codec context
  AVCodecContext  *pCodecCtx = NULL;

  struct SwsContext *sws_ctx = NULL;

  AVCodec         *pCodec = NULL; // the codecer
  AVFrame         *pFrame = NULL;
  AVPacket        packet;

  int             frameFinished;
  float           aspect_ratio;

  AVPicture  	  *pict  = NULL;

  SDL_Rect        rect;
  Uint32 	  pixformat; 

  //for render
  SDL_Window 	  *win = NULL;
  SDL_Renderer    *renderer = NULL;
  SDL_Texture     *texture = NULL;

  //set defualt size of window 
  int w_width = 640;
  int w_height = 480;


  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    //fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL - %s\n", SDL_GetError());
    return;
  }

  //Register all formats and codecs
  //av_register_all();

  // Open video file
  if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0){
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open video file!");
    goto __FAIL; // Couldn't open file
  }
  
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL)<0){
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find stream infomation!");
    goto __FAIL; // Couldn't find stream information
  }
  
  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, filename, 0);
  
  // Find the first video stream
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++) {
    if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  }

  if(videoStream==-1){
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Din't find a video stream!");
    goto __FAIL;// Didn't find a video stream
  }
  
  // Get a pointer to the codec context for the video stream
  //pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
  
  // Find the decoder for the video stream
  //pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
  pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
  if(pCodec==NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported codec!\n");
    goto __FAIL; // Codec not found
  }

  pCodecCtxOrig = avcodec_alloc_context3(pCodec);
  ret = avcodec_parameters_to_context(pCodecCtxOrig, pFormatCtx->streams[videoStream]->codecpar);
  if (ret < 0)
  {
	  printf("find context failed from codecpar...");
	  return;
  }

  // Copy context
  //pCodecCtx = avcodec_alloc_context3(pCodec);

  //if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
  //  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,  "Couldn't copy codec context");
  //  goto __FAIL;// Error copying codec context
  //}

  // Open codec
  if(avcodec_open2(pCodecCtxOrig, pCodec, NULL)<0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open decoder!\n");
    goto __FAIL; // Could not open codec
  }
  
  // Allocate video frame
  pFrame=av_frame_alloc();

  w_width = pCodecCtxOrig->width;
  w_height = pCodecCtxOrig->height;

  win = SDL_CreateWindow( "Media Player",
		          SDL_WINDOWPOS_UNDEFINED,
		  	  SDL_WINDOWPOS_UNDEFINED,
			  w_width, w_height,
			  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);	  
  if(!win){
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window by SDL");  
    goto __FAIL;
  }

  renderer = SDL_CreateRenderer(win, -1, 0);
  if(!renderer){
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Renderer by SDL");  
    goto __FAIL;
  }

  pixformat = SDL_PIXELFORMAT_IYUV;
  texture = SDL_CreateTexture(renderer,
		    pixformat, 
		    SDL_TEXTUREACCESS_STREAMING,
		    w_width, 
		    w_height);

  // initialize SWS context for software scaling
  //sws_alloc_context();
  /*sws_ctx = sws_getContext(pCodecCtx->width,
			   pCodecCtx->height,
			   pCodecCtx->pix_fmt,
			   pCodecCtx->width,
			   pCodecCtx->height,
			   AV_PIX_FMT_YUV420P,
			   SWS_BILINEAR,
			   NULL,
			   NULL,
			   NULL
			   );*/

  /*pict = (AVPicture*)malloc(sizeof(AVPicture));
  avpicture_alloc(pict, 
		  AV_PIX_FMT_YUV420P, 
		  pCodecCtx->width, 
		  pCodecCtx->height);
*/

  // Read frames and save first five frames to disk
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
	  // Is this a packet from the video stream?
	  if (packet.stream_index == videoStream) {
		  // Decode video frame
		  //avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
		  ret = avcodec_send_packet(pCodecCtxOrig, &packet);
		  if (ret < 0) {
			  fprintf(stderr, "Error sending the frame to the encoder\n");
			  return;
		  }

		  while (ret >= 0) {
			  ret = avcodec_receive_frame(pCodecCtxOrig, pFrame);
			  if (ret == AVERROR_EOF)
			  {
				  return;
			  }
			  else if (ret == AVERROR(EAGAIN))
			  {
				  break;
			  }
			  else if (ret < 0)
			  {
				  fprintf(stderr, "Error encoding audio frame\n");
				  return;
			  }

			  // Did we get a video frame?
			  if (ret >= 0) {

				  // Convert the image into YUV format that SDL uses
				  /*sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
						pFrame->linesize, 0, pCodecCtx->height,
						pict->data, pict->linesize);*/

						/*SDL_UpdateYUVTexture(texture, NULL,
									 pict->data[0], pict->linesize[0],
									 pict->data[1], pict->linesize[1],
									 pict->data[2], pict->linesize[2]);*/
				  SDL_UpdateYUVTexture(texture, NULL,
					  pFrame->data[0], pFrame->linesize[0],
					  pFrame->data[1], pFrame->linesize[1],
					  pFrame->data[2], pFrame->linesize[2]);

				  // Set Size of Window
				  rect.x = 0;
				  rect.y = 0;
				  rect.w = pCodecCtxOrig->width;
				  rect.h = pCodecCtxOrig->height;

				  SDL_RenderClear(renderer);
				  SDL_RenderCopy(renderer, texture, NULL, &rect);
				  SDL_RenderPresent(renderer);
				  //SDL_Delay(41);

			  }
		  }

		  // Free the packet that was allocated by av_read_frame
		  av_packet_unref(&packet);

		  
		  SDL_Event event;
		  SDL_PollEvent(&event);
		  switch(event.type) {
		  case SDL_QUIT:
			goto __QUIT;
			break;
		  default:
			break;
		  }
		  

	  }
  }

__QUIT:
  ret = 0;
  
__FAIL:
  // Free the YUV frame
  if(pFrame){
    av_frame_free(&pFrame);
  }
  
  // Close the codec
  if(pCodecCtx){
    avcodec_close(pCodecCtx);
  }

  if(pCodecCtxOrig){
    avcodec_close(pCodecCtxOrig);
  }
  
  // Close the video file
  if(pFormatCtx){
    avformat_close_input(&pFormatCtx);
  }

  if(pict){
    //avpicture_free(pict);
    free(pict);
  }

  if(win){
    SDL_DestroyWindow(win);
  }

  if(renderer){
    SDL_DestroyRenderer(renderer);
  }

  if(texture){
    SDL_DestroyTexture(texture);
  }

  SDL_Quit();
  
  return;
}
