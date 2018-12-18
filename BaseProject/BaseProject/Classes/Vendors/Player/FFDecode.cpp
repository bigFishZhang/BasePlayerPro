//
// Created by bigfish on 2018/10/12.
//
extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavcodec/jni.h>
//    include "libswscale/swscale.h"
#include <libswscale/swscale.h>
}



#include "FFDecode.h"
#include "ZLog.h"


void FFDecode::InitHard(void *vm)
{
    av_jni_set_java_vm(vm,0);
}


void  FFDecode::Clear()
{
    IDecode::Clear();
    mux.lock();
    if(codec)
    {
        avcodec_flush_buffers(codec);
    }
    mux.unlock();
}

void FFDecode::Close()
{
    IDecode::Clear();
    mux.lock();
    pts = 0;
    if(frame)
        av_frame_free(&frame);
    if(codec)
    {
        avcodec_close(codec);
        avcodec_free_context(&codec);
    }
    sws_freeContext(img_convert_ctx);
    mux.unlock();
}

bool FFDecode::Open(ZParameter para , bool isHard)
{
    Close();
    if(!para.para) return false;
    AVCodecParameters *p = para.para;

    //1 查找解码器
    AVCodec *cd = avcodec_find_decoder(p->codec_id);
    if(isHard)
    {
        cd = avcodec_find_decoder_by_name("h264_mediacodec");
    }

    if(!cd)
    {
        ZLOGE("avcodec_find_decoder %d failed!  %d",p->codec_id,isHard);
        return false;
    }
    ZLOGI("avcodec_find_decoder success %d!",isHard);
    
    mux.lock();
    //2 创建解码上下文，并复制参数
    codec = avcodec_alloc_context3(cd);
    avcodec_parameters_to_context(codec,p);

    codec->thread_count = 8;
    //3 打开解码器
    int re = avcodec_open2(codec,0,0);
    if(re != 0)
    {
        mux.unlock();
        char buf[1024] = {0};
        av_strerror(re,buf,sizeof(buf)-1);
        ZLOGE("%s",buf);
        return false;
    }

    if(codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        this->isAudio = false;
    }
    else
    {
        this->isAudio = true;
    }
    mux.unlock();
    ZLOGI("avcodec_open2 success!");
    return true;
}


bool FFDecode::SendPacket(ZData pkt)
{
    if(pkt.size<=0 || !pkt.data)return false;
    mux.lock();
    if(!codec)
    {
        mux.unlock();
        return false;
    }
    int re = avcodec_send_packet(codec,(AVPacket*)pkt.data);
    mux.unlock();
    if(re != 0)
    {
        return false;
    }
    return true;
}

//bool FFDecode::


//从线程中获取解码结果
ZData FFDecode::RecvFrame()
{
    mux.lock();
  
    if(!codec)
    {
        mux.unlock();
        return ZData();
    }
    if(!frame)
    {
        frame = av_frame_alloc();
        
    }
    if (!pFrameYUV) {
        pFrameYUV = av_frame_alloc();
       
    }
    int re = avcodec_receive_frame(codec,frame);
    if(re != 0)
    {
        mux.unlock();
        return ZData();
    }
    ZData d;
    d.data = (unsigned char *)frame;
    
    out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, codec->width, codec->height));
    //设置图像内容
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, codec->width, codec->height);
    
 

    if(codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
       
//        SwsScale();

        img_convert_ctx = sws_getContext(codec->width, codec->height, codec->pix_fmt,
                                         codec->width, codec->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
        
        
        sws_scale(img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, codec->height, pFrameYUV->data, pFrameYUV->linesize);
       
        
        d.size = (frame->linesize[0] + frame->linesize[1] + frame->linesize[2])*frame->height;
        d.width = frame->width;
        d.height = frame->height;
      
        memcpy(d.datas,pFrameYUV->data,sizeof(d.datas));
        d.format = frame->format;
        d.pts = frame->pts;
        pts = d.pts;
    }
    else
    {
        //样本字节数 * 单通道样本数 * 通道数
        d.size = av_get_bytes_per_sample((AVSampleFormat)frame->format)*frame->nb_samples*2;
         memcpy(d.datas,frame->data,sizeof(d.datas));
        d.format = frame->format;
        d.pts  = frame->pts;
        pts    = d.pts;
    }
    
    //if(!isAudio)
    //    XLOGE("data format is %d",frame->format);
   
  
    mux.unlock();
    return d;
}

void FFDecode::SwsScale()
{
   
 

//    sws_freeContext(img_convert_ctx);
     img_convert_ctx = sws_getContext(codec->width, codec->height, codec->pix_fmt,
                                     codec->width, codec->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


//    if(codec->codec_type == AVMEDIA_TYPE_VIDEO)
//    {
        //上文说的对图形进行宽度上方的裁剪，以便于显示的更好
      int re = sws_scale(img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, codec->height, pFrameYUV->data, pFrameYUV->linesize);
        if (re < 0) {
            printf("%d",re);
        }
//    }
//    sws_freeContext(img_convert_ctx);
}
