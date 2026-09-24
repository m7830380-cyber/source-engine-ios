/**************************************************************************

Filename    :   DDS_ImageReader.cpp
Content     :   DDS Image file format reader implementation
Created     :   February 2010
Authors     :   Michael Antonov, Artem Bolgar

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.
                     Copyright 2026 Final Game Production Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "DDS_ImageFile.h"
#include "Render_ImageFileUtil.h"
#include "Kernel/SF_File.h"
#include "Kernel/SF_Debug.h"
#include "Kernel/SF_HeapNew.h"
#include <stdio.h>
#include <string.h>
#ifdef SF_OS_XBOX360
#include <xgraphics.h>
#endif
#ifdef SF_OS_ANDROID
#include "Render/GL/GL_Common.h"
#endif

namespace Scaleform { namespace Render { namespace DDS {

// DDSD (DirectDraw Surface Descriptor) constants.
enum DDSFlags
{
    DDSF_CAPS               =0x00000001l,
    DDSF_HEIGHT             =0x00000002l,
    DDSF_WIDTH              =0x00000004l,
    DDSF_PITCH              =0x00000008l,
    DDSF_BACKBUFFERCOUNT    =0x00000020l,
    DDSF_ZBUFFERBITDEPTH    =0x00000040l,
    DDSF_ALPHABITDEPTH      =0x00000080l,
    DDSF_LPSURFACE          =0x00000800l,
    DDSF_PIXELFORMAT        =0x00001000l,
    DDSF_CKDESTOVERLAY      =0x00002000l,
    DDSF_CKDESTBLT          =0x00004000l,
    DDSF_CKSRCOVERLAY       =0x00008000l,
    DDSF_CKSRCBLT           =0x00010000l,
    DDSF_MIPMAPCOUNT        =0x00020000l,
    DDSF_REFRESHRATE        =0x00040000l,
    DDSF_LINEARSIZE         =0x00080000l,
    DDSF_TEXTURESTAGE       =0x00100000l,
    DDSF_FVF                =0x00200000l,
    DDSF_SRCVBHANDLE        =0x00400000l,
    DDSF_DEPTH              =0x00800000l
};

//
enum DDSPixelFormat
{
    DDSPF_ALPHAPIXELS       =0x00000001l,
    DDSPF_ALPHA             =0x00000002l,
    DDSPF_FOURCC            =0x00000004l,
    DDSPF_PALETTEINDEXED4   =0x00000008l,
    DDSPF_PALETTEINDEXEDTO8 =0x00000010l,
    DDSPF_PALETTEINDEXED8   =0x00000020l,
    DDSPF_RGB               =0x00000040l,
    DDSPF_COMPRESSED        =0x00000080l
};

// In case this platform doesn't have this macro.
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
    ((unsigned)(UByte)(ch0) | ((unsigned)(UByte)(ch1) << 8) |       \
    ((unsigned)(UByte)(ch2) << 16) | ((unsigned)(UByte)(ch3) << 24 ))
#endif // MAKEFOURCC)

struct DDSDescr
{
    UInt32    RGBBitCount;
    UInt32    RBitMask;
    UInt32    GBitMask;
    UInt32    BBitMask;
    UInt32    ABitMask;
    bool      HasAlpha;
    UByte     ShiftR, ShiftG, ShiftB, ShiftA;
    inline DDSDescr()
    {
        RGBBitCount = RBitMask = GBitMask = BBitMask = ABitMask = 0;
        ShiftR = ShiftG = ShiftB = ShiftA;
        HasAlpha = false;                
    }

    static UByte CalcShiftByMask(UInt32 mask)
    {
        unsigned shifts = 0;

        if (mask == 0) return 0;

        if ((mask & 0xFFFFFFu) == 0)
        {
            mask >>= 24;
            shifts += 24;
        }
        else if ((mask & 0xFFFFu) == 0)
        {
            mask >>= 16;
            shifts += 16;
        }
        else if ((mask & 0xFFu) == 0)
        {
            mask >>= 8;
            shifts += 8;
        }
        while((mask & 1) == 0)
        {
            mask >>= 1;
            ++shifts;
        }
        return UByte(shifts);
    }

    void CalcShifts()
    {
        ShiftR = CalcShiftByMask(RBitMask);
        ShiftG = CalcShiftByMask(GBitMask);
        ShiftB = CalcShiftByMask(BBitMask);
        ShiftA = CalcShiftByMask(ABitMask);
    }
};

struct DDSHeaderInfo
{
    UInt32      Width;
    UInt32      Height;
    UInt32      Pitch;
    ImageFormat Format;
    UInt32      MipmapCount;
    DDSDescr    DDSFmt;
    bool        OppositeEndian;   // For X360, if the DDS is little endian, we need to know so we can byte swap the data.

    DDSHeaderInfo() : Width(0), Height(0), Pitch(0), Format(Image_None), MipmapCount(1), DDSFmt(), OppositeEndian(false) {}

};

struct DDSDXT10Info
{
    SInt32      DXGIFormat;
    UInt32      ResourceDimension;
    UInt32      MiscFlag; // see D3D11_RESOURCE_MISC_FLAG
    UInt32      ArraySize;
    UInt32      Reserved;
};

// Temporary DDS image class used as a data source, allowing
// direct initialization of RawImage or Texture with image data.

class DDSFileImageSource : public FileImageSource
{
    UByte           ImageDesc; 
    DDSHeaderInfo   HeaderInfo;
    bool            DecodeDXT;  // iOS: DXT data decoded to RGBA while reading

    bool DecodeDXTData(ImageData* pdest, CopyScanlineFunc copyScanline, void* arg) const;
    
public:
    DDSFileImageSource(File* file, ImageFormat format)
        : FileImageSource(file,format), ImageDesc(0), DecodeDXT(false)
    {
    }
    virtual      ~DDSFileImageSource(){}
    bool ReadHeader();
    virtual bool Decode(ImageData* pdest, CopyScanlineFunc copyScanline, void* arg) const;
    virtual unsigned        GetMipmapCount() const { return HeaderInfo.MipmapCount; }
};


// *** DDS Format loading

static const UByte* ParseUInt32(const UByte* buf, UInt32* pval)
{
    *pval = Alg::ByteUtil::LEToSystem(*(UInt32*)buf);    
    return buf + 4;
}



static bool Image_ParseDDSHeader(DDSHeaderInfo* pinfo, const UByte* buf, const UByte** pdata)
{
    ImagePlane p0;
    UInt32 flags;
    UInt32 v;
    
    buf = ParseUInt32(buf, &flags);

    buf = ParseUInt32(buf, &v);
    if (flags & DDSF_HEIGHT)
        pinfo->Height = v;

    buf = ParseUInt32(buf, &v);
    if (flags & DDSF_WIDTH)
        pinfo->Width = v;
    buf = ParseUInt32(buf, &v);
    if (flags & DDSF_PITCH)
        pinfo->Pitch = v;
    else if (flags & DDSF_LINEARSIZE)
        pinfo->Pitch = v/pinfo->Height*4; // Required by D3D10

    buf = ParseUInt32(buf, &v);
    //if (flags & DDSF_DEPTH)
    //    pimage->Depth = v;

    buf = ParseUInt32(buf, &v);
    if (flags & DDSF_MIPMAPCOUNT)
        pinfo->MipmapCount = v;

    //buf = ParseUInt32(buf, &v); // alpha bit count
    //if (flags & DDSF_ALPHABITDEPTH)
    //    pimage->AlphaBitDepth = v;

    buf += 11 * 4;

    if (flags & DDSF_PIXELFORMAT)
    {
        // pixel format (DDPIXELFORMAT)
        buf = ParseUInt32(buf, &v); // dwSize
        if (v != 32) // dwSize should be == 32 
        {
            SF_ASSERT(0);
            return false;
        }
      
        UInt32 pfflags;
        buf = ParseUInt32(buf, &pfflags);   // dwFlags
        buf = ParseUInt32(buf, &v);         // dwFourCC
        if (pfflags & DDSPF_FOURCC)
        {
            if (v == MAKEFOURCC('D','X','T','5'))        // DXT5
                pinfo->Format = Image_DXT5;
            else if (v == MAKEFOURCC('D','X','T','3'))   // DXT3
                pinfo->Format = Image_DXT3;
            else if (v == MAKEFOURCC('D','X','T','1'))   // DXT1
                pinfo->Format = Image_DXT1;
            else if (v == MAKEFOURCC('D','X','T','2')|| 
                     v == MAKEFOURCC('D','X','T','4') )  // DXT2 & DXT4 - not supported.
            {
                SF_DEBUG_WARNING(1, "DXT2 and DXT4 are not supported.\n");
                return false;
            }
            // ATC textures are stored in DDS files.
#ifdef GL_AMD_compressed_ATC_texture
            else if ( v == MAKEFOURCC('A','T','C',' '))
            {
                pinfo->Format = Image_ATCIC;
            }
            else if ( v == MAKEFOURCC('A','T','C','A'))
            {
                pinfo->Format = Image_ATCICA;
            }
            else if ( v == MAKEFOURCC('A','T','C','I'))
            {
                pinfo->Format = Image_ATCICI;
            }
#endif
            else if ( v == MAKEFOURCC('D','X','1','0'))
            {
                pinfo->Format = Image_BC7;
            }
            else
            {
                SF_DEBUG_WARNING4(1, "Unrecognized DDS FourCC code: %c%c%c%c\n", 
                    ((char*)&v)[0], ((char*)&v)[1], ((char*)&v)[2], ((char*)&v)[3] );
                return false;
            }
            buf += 20;  // skip remaining part of PixelFormat
        }
        else if ((pfflags & DDSPF_RGB) || (pfflags & DDSPF_ALPHA)) 
        {
            // uncompressed DDS. Only 32-bit/24-bit RGB formats and alpha only (A8) are supported
            UInt32 bitCount;
            buf = ParseUInt32(buf, &bitCount); // dwRGBBitCount
            pinfo->DDSFmt.RGBBitCount = bitCount;
            switch(bitCount)
            {
                case 32: pinfo->Format = Image_R8G8B8A8; break;
                case 24: pinfo->Format = Image_R8G8B8; break;
                case 8:
                    if (pfflags & DDSPF_ALPHA)
                    {
                        pinfo->Format = Image_A8;
                        break;
                    }
                default:
                    SF_ASSERT(0); // unsupported
                    return false;
            }
            if (!(flags & DDSF_PITCH))
                pinfo->Pitch = pinfo->Width*(bitCount/8);
                //AB: what is the Pitch in DDS for 24-bit RGB?
            
            buf = ParseUInt32(buf, &v); // dwRBitMask
            pinfo->DDSFmt.RBitMask = v;
            buf = ParseUInt32(buf, &v); // dwGBitMask
            pinfo->DDSFmt.GBitMask = v;
            buf = ParseUInt32(buf, &v); // dwBBitMask
            pinfo->DDSFmt.BBitMask = v;
            buf = ParseUInt32(buf, &v); // dwRGBAlphaBitMask
            if (pfflags & DDSPF_ALPHAPIXELS) 
            {
                pinfo->DDSFmt.ABitMask = v;
                pinfo->DDSFmt.HasAlpha = true;
            }

            // check for X8R8G8B8 - need to set alpha to 255
            if (v == 0 && bitCount == 32)
            {
                SF_ASSERT(0); // not supported for now.
                //@TODO - need to have one more Image_<> format for X8R8G8B8
            }
        }
        SF_ASSERT(pinfo->Format != Image_None); // Unsupported format
        if (pinfo->Format == Image_None)
            return false;
    }
    else
        buf += 32;
    buf += 16; // skip ddsCaps
    buf += 4; // skip reserved
    if (pdata) *pdata = buf;
    return true;
}

static bool ProcessUDDSData(UByte* buffer, UPInt size, ImageFormat format, const DDSDescr& ddsFmt)
{   
    if (format == Image_R8G8B8)
    {
        for (unsigned x = 0; x < size; x += 3)
        {
            UInt32 val = buffer[x + 0] | (UInt32(buffer[x + 1]) << 8) | (UInt32(buffer[x + 2]) << 16);
            buffer[x + 2] = UByte((val >> ddsFmt.ShiftB) & 0xFF); // B
            buffer[x + 1] = UByte((val >> ddsFmt.ShiftG) & 0xFF); // G
            buffer[x + 0] = UByte((val >> ddsFmt.ShiftR) & 0xFF); // R
        }
    }
    else if (format == Image_R8G8B8A8)
    {
        for (unsigned x = 0; x < size; x += 4)
        {
            UInt32 val = buffer[x + 0] | (UInt32(buffer[x + 1]) << 8) |
                (UInt32(buffer[x + 2]) << 16) | (UInt32(buffer[x + 3]) << 24);
            buffer[x + 2] = UByte((val >> ddsFmt.ShiftB) & 0xFF); // B
            buffer[x + 1] = UByte((val >> ddsFmt.ShiftG) & 0xFF); // G
            buffer[x + 0] = UByte((val >> ddsFmt.ShiftR) & 0xFF); // R
            if (ddsFmt.HasAlpha)
                buffer[x + 3] = UByte((val >> ddsFmt.ShiftA) & 0xFF); // A
            else
                buffer[x + 3] = 0xFF;
        }
    }

    return true;
}


// ***** DXT decoding (iOS: the GPU has no BC/DXT texture support)

static void DXT_DecodeColorBlock(const UByte* pblock, UByte* prgba, bool dxt1)
{
    UInt16 c0 = UInt16(pblock[0] | (pblock[1] << 8));
    UInt16 c1 = UInt16(pblock[2] | (pblock[3] << 8));
    UByte colors[4][4];
    colors[0][0] = UByte(((c0 >> 11) & 31) * 255 / 31);
    colors[0][1] = UByte(((c0 >> 5) & 63) * 255 / 63);
    colors[0][2] = UByte((c0 & 31) * 255 / 31);
    colors[0][3] = 255;
    colors[1][0] = UByte(((c1 >> 11) & 31) * 255 / 31);
    colors[1][1] = UByte(((c1 >> 5) & 63) * 255 / 63);
    colors[1][2] = UByte((c1 & 31) * 255 / 31);
    colors[1][3] = 255;
    if (!dxt1 || c0 > c1)
    {
        for (int k = 0; k < 3; k++)
        {
            colors[2][k] = UByte((2 * colors[0][k] + colors[1][k]) / 3);
            colors[3][k] = UByte((colors[0][k] + 2 * colors[1][k]) / 3);
        }
        colors[2][3] = colors[3][3] = 255;
    }
    else
    {
        for (int k = 0; k < 3; k++)
        {
            colors[2][k] = UByte((colors[0][k] + colors[1][k]) / 2);
            colors[3][k] = 0;
        }
        colors[2][3] = 255;
        colors[3][3] = 0;   // DXT1 1-bit alpha: transparent black
    }
    UInt32 bits = UInt32(pblock[4]) | (UInt32(pblock[5]) << 8) | (UInt32(pblock[6]) << 16) | (UInt32(pblock[7]) << 24);
    for (int i = 0; i < 16; i++)
    {
        const UByte* pc = colors[(bits >> (2 * i)) & 3];
        UByte* pd = prgba + i * 4;
        pd[0] = pc[0]; pd[1] = pc[1]; pd[2] = pc[2]; pd[3] = pc[3];
    }
}

// Decodes one 4x4 block into prgba (16 pixels, row-major, RGBA8).
static void DXT_DecodeBlock(ImageFormat format, const UByte* pblock, UByte* prgba)
{
    if (format == Image_DXT1)
    {
        DXT_DecodeColorBlock(pblock, prgba, true);
        return;
    }
    DXT_DecodeColorBlock(pblock + 8, prgba, false);
    if (format == Image_DXT3)
    {
        for (int i = 0; i < 16; i++)
        {
            UByte a4 = UByte((pblock[i / 2] >> ((i & 1) * 4)) & 15);
            prgba[i * 4 + 3] = UByte(a4 * 17);
        }
    }
    else // DXT5
    {
        UByte a[8];
        a[0] = pblock[0];
        a[1] = pblock[1];
        if (a[0] > a[1])
            for (int k = 1; k < 7; k++) a[k + 1] = UByte(((7 - k) * a[0] + k * a[1]) / 7);
        else
        {
            for (int k = 1; k < 5; k++) a[k + 1] = UByte(((5 - k) * a[0] + k * a[1]) / 5);
            a[6] = 0;
            a[7] = 255;
        }
        UInt64 abits = 0;
        for (int k = 0; k < 6; k++)
            abits |= UInt64(pblock[2 + k]) << (8 * k);
        for (int i = 0; i < 16; i++)
            prgba[i * 4 + 3] = a[(abits >> (3 * i)) & 7];
    }
}

bool DDSFileImageSource::DecodeDXTData(ImageData* pdest, CopyScanlineFunc copyScanline, void* arg) const
{
    const ImageFormat dxtFormat = HeaderInfo.Format;
    const unsigned blockBytes = (dxtFormat == Image_DXT1) ? 8 : 16;

    unsigned mipWidth = Size.Width;
    unsigned mipHeight = Size.Height;
    for (unsigned m = 0; m < pdest->GetMipLevelCount(); m++)
    {
        ImagePlane mipPlane;
        if (pdest->HasSeparateMipmaps())
            pdest->GetMipLevelPlane(m, 0, &mipPlane);
        else
            pdest->GetPlaneRef().GetMipLevel(pdest->GetFormat(), m, &mipPlane);

        const unsigned blocksW = (mipWidth + 3) / 4;
        const unsigned blocksH = (mipHeight + 3) / 4;
        const UPInt rowBytes = UPInt(blocksW) * blockBytes;
        const UPInt outPitch = UPInt(blocksW) * 4 * 4;   // 4 pixels per block, RGBA
        UByte* pblocks = (UByte*)SF_ALLOC(rowBytes, Stat_Default_Mem);
        UByte* prows = (UByte*)SF_ALLOC(outPitch * 4, Stat_Default_Mem);
        if (!pblocks || !prows)
        {
            if (pblocks) SF_FREE(pblocks);
            if (prows) SF_FREE(prows);
            return false;
        }

        bool ok = true;
        for (unsigned by = 0; by < blocksH && ok; by++)
        {
            if (pFile->Read(pblocks, (int)rowBytes) != (int)rowBytes)
            {
                ok = false;
                break;
            }
            UByte block[16 * 4];
            for (unsigned bx = 0; bx < blocksW; bx++)
            {
                DXT_DecodeBlock(dxtFormat, pblocks + bx * blockBytes, block);
                for (unsigned r = 0; r < 4; r++)
                    memcpy(prows + r * outPitch + bx * 16, block + r * 16, 16);
            }
            for (unsigned r = 0; r < 4; r++)
            {
                unsigned y = by * 4 + r;
                if (y < mipHeight)
                    copyScanline(mipPlane.GetScanline(y), prows + r * outPitch, UPInt(mipWidth) * 4, 0, arg);
            }
        }
        SF_FREE(pblocks);
        SF_FREE(prows);
        if (!ok)
            return false;

        mipWidth  = Alg::Max(1u, mipWidth / 2); 
        mipHeight = Alg::Max(1u, mipHeight / 2); 
    }
    return true;
}

bool DDSFileImageSource::Decode( ImageData* pdest, CopyScanlineFunc copyScanline, void* arg ) const
{
    if (!seekFileToDecodeStart())
        return false;

    if (DecodeDXT)
        return DecodeDXTData(pdest, copyScanline, arg);

    unsigned mipWidth = Size.Width;
    unsigned mipHeight = Size.Height;
    for (unsigned m = 0; m < pdest->GetMipLevelCount(); m++)
    {
        ImagePlane mipPlane;
        if (pdest->HasSeparateMipmaps())
            pdest->GetMipLevelPlane(m, 0, &mipPlane);
        else
            pdest->GetPlaneRef().GetMipLevel(pdest->GetFormat(), m, &mipPlane);

        ImageScanlineBuffer<1024*4> scanline(HeaderInfo.Format, mipWidth, Format);
        int readSize = (int)scanline.GetReadSize();
        if (!scanline.IsValid())
            return false;
        for (unsigned y = 0; y < ImageData::GetFormatScanlineCount(Format, mipHeight); y++)
        {
            if (pFile->Read(scanline.GetReadBuffer(), readSize) != readSize)
                return false;
            if (Format == Image_R8G8B8A8 || Format == Image_R8G8B8)
                ProcessUDDSData(scanline.GetReadBuffer(), readSize, Format, HeaderInfo.DDSFmt);
            UByte* destScanline = mipPlane.GetScanline(y);
            scanline.ConvertReadBuffer(destScanline, 0, copyScanline, arg);
#ifdef SF_OS_XBOX360
            if ( HeaderInfo.OppositeEndian && (Format >= Image_DXT1 && Format <= Image_DXT5 ))
                XGEndianSwapMemory(destScanline, destScanline, XGENDIAN_8IN16, sizeof(UInt16), readSize/2);
#endif
        }

        mipWidth  = Alg::Max(1u, mipWidth / 2); 
        mipHeight = Alg::Max(1u, mipHeight / 2); 
    }
    return true;

}

bool DDSFileImageSource::ReadHeader()
{
    UInt32  fourcc;
    bool    oppEndian = false;

    // Read without endian swapping.
    pFile->Read((UByte*)&fourcc, sizeof(UInt32));
    if (fourcc != MAKEFOURCC('D','D','S',' '))
    {
        // Perhaps it is just the wrong endianness for this platform.
        fourcc = (UInt32)Alg::ByteUtil::LEToSystem(fourcc);
        oppEndian = true;
    }

    if ( fourcc != MAKEFOURCC('D','D','S',' '))
        return false;

    UInt32 sz = pFile->ReadUInt32();
    if (sz != 124)
        return false;
    UByte buf[256];
    if (pFile->Read(buf, 120) != 120)
        return false;

    if (!Image_ParseDDSHeader(&HeaderInfo, buf, 0))
        return false;
    if (HeaderInfo.Format == Image_BC7)
        pFile->Seek(sizeof(DDSDXT10Info), File::Seek_Cur);
    HeaderInfo.OppositeEndian = oppEndian;
    HeaderInfo.DDSFmt.CalcShifts();

    if (Format == Image_None)
        Format =  HeaderInfo.Format;
#if defined(SF_USE_ANGLE)
    // No DXT textures on iPhone GPUs: hand out RGBA and decode while reading.
    if (HeaderInfo.Format == Image_DXT1 || HeaderInfo.Format == Image_DXT3 || HeaderInfo.Format == Image_DXT5)
    {
        DecodeDXT = true;
        Format = Image_R8G8B8A8;
    }
#endif
#if defined(SF_USE_ANGLE)
    printf("[sf-dds] %ux%u, file format %d, mips %u, decoding DXT %d\n", HeaderInfo.Width, HeaderInfo.Height,
           (int)HeaderInfo.Format, (unsigned)HeaderInfo.MipmapCount, DecodeDXT ? 1 : 0);
    fflush(stdout);
#endif
    Size = ImageSize(HeaderInfo.Width, HeaderInfo.Height);
    FilePos = pFile->LTell();
    return true;
}
bool FileReader::MatchFormat(File* file, UByte* headerArg, UPInt headerArgSize) const
{
    FileHeaderReader<4> header(file, headerArg, headerArgSize);
    if (!header)
        return false;
    if ((header[0] != 'D') || (header[1] != 'D') || (header[2] != 'S') || (header[3] != ' '))
        return false;
    return true;
}

ImageSource* FileReader::ReadImageSource(File* file, const ImageCreateArgs& args) const
{
    if (!file || !file->IsValid())
        return 0;

    DDSFileImageSource* source = SF_NEW DDSFileImageSource(file, args.Format);
    if (source && !source->ReadHeader())
    {
        source->Release();
        return 0;
    }
    return source; 
   
}

// Instance singleton.
FileReader FileReader::Instance;

}}}

