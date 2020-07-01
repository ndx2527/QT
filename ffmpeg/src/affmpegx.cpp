#include "affmpeg.h"
#include "unistd.h"

#define _MAXAUDIOFRAMESIZE_ 19200
#define _MAXDEVIATION_ 0.1

AFFmpeg::AFFmpeg()
{
    videoindex = -1;
    audioindex = -1;
    duration = 0;
    cduration = 0;
    ABufLen = 0;
    isopen = false;
    isstart = false;
    isaudio = true;
    av_register_all();

    //video
    pFormatContext = avformat_alloc_context();
    pVCPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    pVideoFrame = av_frame_alloc();
    pVideoFrameRGB = av_frame_alloc();

    //audio
    pAFormatContext = avformat_alloc_context();
    pACPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    pAudioFrame = av_frame_alloc();

    //filter
    pAFilter = new AFilter;
    pFilterFrame = av_frame_alloc();
}

AFFmpeg::~AFFmpeg()
{
    av_free(out_buffer);
    sws_freeContext(pSwsContext);
    av_frame_free(&pVideoFrame);
    av_frame_free(&pVideoFrameRGB);
    avcodec_close(pVideoCodecContext);

    avformat_close_input(&pFormatContext);
    avformat_free_context(pFormatContext);

    if(isaudio)
    {
        av_free(pAudioBuffer);
        swr_free(&pSwrContext);
        av_frame_free(&pAudioFrame);
        avcodec_close(pAudioCodecContext);

        avformat_close_input(&pAFormatContext);
        avformat_free_context(pAFormatContext);
    }

    delete pAFilter;
    av_frame_free(&pFilterFrame);
}

/*************************************************************
 * 从头文件获取相关信息,听说有些头文件是不标准的，所以一些信息可能不标准
**************************************************************/
bool AFFmpeg::open(const char *filepath,bool open)
{
    isaudio = open;

    //初始化pFormatContext
    if(avformat_open_input(&pFormatContext,filepath,NULL,NULL) != 0)
    {
        printf("Couldn't open input stream.%s\n",filepath);
        return false;
    }

    //获取音视频流数据信息
    if(avformat_find_stream_info(pFormatContext,NULL) < 0)
    {
        printf("Couldn't find stream information.\n");
        return false;
    }
    //查找视频流,音频流
    for(int i=0;i<pFormatContext->nb_streams;i++)
    {
        if(pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }

    if(videoindex == -1)
    {
        printf("Didn't find a video stream.\n");
        return false;
    }
    //打印文件相关信息
    //av_dump_format(pFormatContext,0,filepath,0);

    //时长
    duration = pFormatContext->duration / AV_TIME_BASE;
    //获取视频时间基准
    Vrational = pFormatContext->streams[videoindex]->time_base;

    //获取帧率
    fps = pFormatContext->streams[videoindex]->avg_frame_rate;

    //获取视频流编码结构 获取解码器
    pVideoCodecContext = pFormatContext->streams[videoindex]->codec;
    pVideoCodec = avcodec_find_decoder(pVideoCodecContext->codec_id);
    if (pVideoCodec == NULL){
        printf("Codec not found.\n");
        return false;
    }

    //初始化解码器
    if (avcodec_open2(pVideoCodecContext, pVideoCodec, NULL) < 0){
        printf("Could not open codec.\n");
        return false;
    }

    //视频缓冲区初始化
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,pVideoCodecContext->width,pVideoCodecContext->height,1));
    av_image_fill_arrays(pVideoFrameRGB->data,pVideoFrameRGB->linesize,out_buffer,
                         AV_PIX_FMT_RGB32,pVideoCodecContext->width,pVideoCodecContext->height,1);

    //格式转换结构体
    pSwsContext = sws_getContext(pVideoCodecContext->width,pVideoCodecContext->height,pVideoCodecContext->pix_fmt,
                                 pVideoCodecContext->width,pVideoCodecContext->height,AV_PIX_FMT_RGB32,
                                 SWS_BICUBIC,NULL,NULL,NULL);

    if(isaudio)
    {
        if(avformat_open_input(&pAFormatContext,filepath,NULL,NULL) != 0)
        {
            printf("Couldn't open input stream.%s\n",filepath);
            return false;
        }
        //获取音视频流数据信息
        if(avformat_find_stream_info(pAFormatContext,NULL) < 0)
        {
            printf("Couldn't find stream information.\n");
            return false;
        }
        //查找视频流,音频流
        for(int i=0;i<pAFormatContext->nb_streams;i++)
        {
            if(pAFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audioindex = i;
                break;
            }
        }
        //获取音频流编码结构 获取解码器
        if(audioindex != -1)
        {
            //获取音频时间基准
            Arational = pAFormatContext->streams[audioindex]->time_base;

            //音频数据缓冲区
            pAudioBuffer = (uint8_t*)av_malloc(_MAXAUDIOFRAMESIZE_ * 2);
            //音频解码器
            pAudioCodecContext = pAFormatContext->streams[audioindex]->codec;
            pAudioCodec = avcodec_find_decoder(pAudioCodecContext->codec_id);
            avcodec_open2(pAudioCodecContext,pAudioCodec,NULL);
            //格式转换结构体
            pSwrContext = swr_alloc();
            pSwrContext = swr_alloc_set_opts(pSwrContext,AV_CH_LAYOUT_STEREO,
                                             AV_SAMPLE_FMT_S16,44100,av_get_default_channel_layout(pAudioCodecContext->channels),
                                             pAudioCodecContext->sample_fmt,pAudioCodecContext->sample_rate,0,NULL);
            swr_init(pSwrContext);
        }
    }

    int ret = pAFilter->setSomeArg("drawtext=fontfile=\\'C\\:/Windows/Fonts/AdobeArabic-Regular.otf\\':fontcolor=red:fontsize=30:text='AutoCatFuuuu':x=100:y=200",Vrational,pVideoCodecContext);
    //int ret = pAFilter->setSomeArg("drawbox=x=100:y=100:w=100:h=100:color=green@0.5",Vrational,pVideoCodecContext);
    //int ret = pAFilter->setSomeArg("drawgrid=width=100:height=100:thickness=2:color=red@0.5",Vrational,pVideoCodecContext);
    //int ret = pAFilter->setSomeArg("boxblur",Vrational,pVideoCodecContext);
    // ret = pAFilter->setSomeArg("movie=\\'D\\:/Picture/tp/9.jpg\\'[wm];[in][wm]overlay=10:10,scale=280:210[out]",Vrational,pVideoCodecContext);
    printf("ret = %d\n",ret);
    printf("%d\n",pVideoCodecContext->pix_fmt);
    fflush(stdout);
    isopen = true;
    return true;
}

/*****************************************
 * 通常都是视频去同步音频的,所以同步操作写在这里
 * 跳帧的过程中视频会有轻微的不流畅,还没想好怎么处理
 *****************************************/
int AFFmpeg::readVideo()
{
    if(!isopen)
        return -1;
    int got_picture = 0;
    int ret = 0;
AGAIN:
    if(av_read_frame(pFormatContext,pVCPacket) == 0)
    {
        if(pVCPacket->stream_index == videoindex)
        {
            if(avcodec_decode_video2(pVideoCodecContext,pVideoFrame,&got_picture,pVCPacket) < 0)
                return -3;                      //解码出错

            if(isaudio && getDeviation() > _MAXDEVIATION_) //音视频时差上限
            {
                av_free_packet(pVCPacket);
                goto AGAIN;
            }
            if(got_picture)
            {
#if 1
                static int xindex = 10;
                char args[128] = {0};
                snprintf(args,sizeof(args),"drawtext=fontfile=\\'C\\:/Windows/Fonts/AdobeArabic-Regular.otf\\':fontcolor=red:fontsize=30:text='中文':x=%d:y=200",xindex);
                xindex += 3;
                delete pAFilter;
                pAFilter = new AFilter;
                pAFilter->setSomeArg(args,Vrational,pVideoCodecContext);

                pVideoFrame->pts = pVideoFrame->best_effort_timestamp;//   av_frame_get_best_effort_timestamp(pVideoFrame);
                if(av_buffersrc_add_frame(pAFilter->getSrcBuffer(),pVideoFrame) == 0)
                {
                        if(av_buffersink_get_frame(pAFilter->getSinkBuffer(),pFilterFrame) >= 0) {
                            //printf("av_buffersink_get_frame success.\n");
                        }
                }

                fflush(stdout);
#endif

                //这里是要使用pts的 但是会出现顺序颠倒的情况 还没想好怎么处理
                //cduration = pVideoFrame->pts * av_q2d(Vrational);
                cduration = pVCPacket->dts * av_q2d(Vrational);
                sws_scale(pSwsContext,(const unsigned char* const*)pFilterFrame->data,pFilterFrame->linesize,0,
                          pVideoCodecContext->height,pVideoFrameRGB->data,pVideoFrameRGB->linesize);

                pVideoFrameRGB->width = pVideoCodecContext->width;
                pVideoFrameRGB->height = pVideoCodecContext->height;

                av_frame_unref(pFilterFrame);//这些一定要释放 不然内存暴涨
                av_free_packet(pVCPacket);
            }
            return 1;
        }
        else
        {
            av_free_packet(pVCPacket);
            goto AGAIN;
        }

    }
    return -2;
}

int AFFmpeg::readAudio()
{
    if(!isopen && !isaudio)
        return -1;

    int got_frame_ptr = 0;
AGAIN:
    if(av_read_frame(pAFormatContext,pACPacket) == 0)
    {
        if(pACPacket->stream_index == audioindex)
        {
            if( avcodec_decode_audio4(pAudioCodecContext,pAudioFrame,&got_frame_ptr,pACPacket) < 0)
                return -3;

            if(got_frame_ptr > 0)
            {
                swr_convert(pSwrContext,&pAudioBuffer,_MAXAUDIOFRAMESIZE_,
                            (const uint8_t **)pAudioFrame->data, pAudioFrame->nb_samples);
                ABufLen = av_samples_get_buffer_size(0,av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                                     pAudioCodecContext->frame_size,AV_SAMPLE_FMT_S16,1);
                av_free_packet(pACPacket);
            }
            return 1;
        }
        else
        {
            av_free_packet(pACPacket);
            goto AGAIN;
        }
    }
    return -2;
}

int AFFmpeg::getAudio(char **buf)
{
    *buf = (char *)pAudioBuffer;
    return ABufLen;
}

/*************************************
 * 跳转会有点复杂
 * *视频跳转
 *      它只能跳转到最近的I帧,要跳转的时间戳可能不是I帧,导致跳转不是很准
 *      av_seek_frame 会去找最近的前I帧
 *      然后从这个I帧开始解码到跳转的时间戳就行了
 *      PS: 如果直接将pVCPacket指向 跳转时间戳的包 视频会花掉的
 * *音频跳转
 *      音频直接将 pACPacket指向 跳转时间戳的包就完事了
 *************************************/

int AFFmpeg::seek(float pos)
{
    int64_t _duration = pos * duration;
    int64_t stamp = av_rescale(_duration,Vrational.den,Vrational.num);

    cduration = _duration;

    if(av_seek_frame(pFormatContext,videoindex,stamp,AVSEEK_FLAG_BACKWARD) >=0)
    {
        int got_picture = 0;
        while(1)
        {
            if(av_read_frame(pFormatContext,pVCPacket) == 0)
            {
                if(pVCPacket->stream_index == videoindex)
                {
                    if(avcodec_decode_video2(pVideoCodecContext,pVideoFrame,&got_picture,pVCPacket) < 0)
                        return -1;
                    if(got_picture)
                    {
                        sws_scale(pSwsContext,(const unsigned char* const*)pVideoFrame->data,pVideoFrame->linesize,0,
                                  pVideoCodecContext->height,pVideoFrameRGB->data,pVideoFrameRGB->linesize);
                        pVideoFrameRGB->width = pVideoCodecContext->width;
                        pVideoFrameRGB->height = pVideoCodecContext->height;
                    }
                    if(_duration <= pVCPacket->dts * av_q2d(Vrational)){
                        av_free_packet(pVCPacket);
                        break;
                    }
                }
                av_free_packet(pVCPacket);
            }
        }
    }

    if(isaudio)
    {
        if(av_seek_frame(pAFormatContext,audioindex,stamp,AVSEEK_FLAG_BACKWARD) >=0)
        {
            int got_picture = 0;
            while(1)
            {
                if(av_read_frame(pAFormatContext,pACPacket) == 0)
                {
                    if(pACPacket->stream_index == audioindex)
                    {
                        if(avcodec_decode_audio4(pAudioCodecContext,pAudioFrame,&got_picture,pACPacket) < 0)
                            return -1;
                        if(got_picture)
                        {
                            swr_convert(pSwrContext,&pAudioBuffer,_MAXAUDIOFRAMESIZE_,
                                        (const uint8_t **)pAudioFrame->data, pAudioFrame->nb_samples);
                            ABufLen = av_samples_get_buffer_size(0,av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                                                 pAudioCodecContext->frame_size,AV_SAMPLE_FMT_S16,1);
                        }
                        if(_duration <= pACPacket->dts * av_q2d(Arational)){
                            av_free_packet(pACPacket);
                            break;
                        }
                    }
                    av_free_packet(pACPacket);
                }
            }
        }
    }

    return pVCPacket->dts * av_q2d(Vrational);
}

double AFFmpeg::getDeviation()
{
    double time = 0;
    if(pVCPacket && pACPacket ){
        time = pACPacket->dts * av_q2d(Arational)- pVCPacket->dts * av_q2d(Vrational);
    }
    return time;
}
