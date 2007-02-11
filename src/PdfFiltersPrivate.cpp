/***************************************************************************
 *   Copyright (C) 2007 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "PdfDefines.h"
#include "PdfFiltersPrivate.h"

#include "PdfDictionary.h"
#include "PdfOutputStream.h"
#include "PdfTokenizer.h"

#define CHUNK               16384 
#define LZW_TABLE_SIZE      4096

namespace PoDoFo {

/** 
 * This structur contains all necessary values
 * for a FlateDecode and LZWDecode Predictor.
 * These values are normally stored in the /DecodeParams
 * key of a PDF dictionary.
 */
struct TFlatePredictorParams {
    TFlatePredictorParams() {
        nPredictor   = 1;
        nColors      = 1;
        nBPC         = 8;
        nColumns     = 1;
        nEarlyChange = 1;
    };

    int nPredictor;
    int nColors;
    int nBPC;
    int nColumns;
    int nEarlyChange;
};


// -------------------------------------------------------
// Hex
// -------------------------------------------------------
void PdfHexFilter::BeginEncode( PdfOutputStream* pOutput )
{
    if( m_pOutputStream ) 
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode has already an output stream. Did you forget to call EndEncode()?" );
    }

    m_pOutputStream = pOutput;
}

void PdfHexFilter::EncodeBlock( const char* pBuffer, long lLen )
{
    if( !m_pOutputStream )
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode was not yet called or EndEncode was called before this method.");
    }

    char data[2];
    while( lLen-- )
    {
        data[0]  = (*pBuffer & 0xF0) >> 4;
        data[0] += (data[0] > 9 ? 'A' - 10 : '0');

        data[1]  = (*pBuffer & 0x0F);
        data[1] += (data[1] > 9 ? 'A' - 10 : '0');

        m_pOutputStream->Write( data, 2 );

        ++pBuffer;
    }
}

void PdfHexFilter::EndEncode()
{
    if( !m_pOutputStream ) 
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode was not yet called or EndEncode was called before this method.");
    }

    m_pOutputStream = NULL;
}
 
void PdfHexFilter::Decode( const char* pInBuffer, long lInLen, char** ppOutBuffer, long* plOutLen, const PdfDictionary* ) const
{
    PdfError eCode;
    int      i      = 0;
    char*    pStart;
    char     hi, low;

    if( !plOutLen || !pInBuffer || !ppOutBuffer )
    {
        RAISE_ERROR( ePdfError_InvalidHandle );
    }

    *ppOutBuffer = static_cast<char*>(malloc( sizeof(char) * (lInLen >> 1) ));
    pStart       = *ppOutBuffer;

    if( !pStart )
    {
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    while( i < lInLen )
    {
        while( PdfTokenizer::IsWhitespace( pInBuffer[i] ) )
            ++i;
        hi  = pInBuffer[i++];

        while( PdfTokenizer::IsWhitespace( pInBuffer[i] ) )
            ++i;
        low = pInBuffer[i++];

        hi  -= ( hi  < 'A' ? '0' : 'A'-10 );
        low -= ( low < 'A' ? '0' : 'A'-10 );

        *pStart = (hi << 4) | (low & 0x0F);
        ++pStart;
    }

    *plOutLen = (pStart - *ppOutBuffer);

}

// -------------------------------------------------------
// Ascii 85
// 
// based on public domain software from:
// Paul Haahr - http://www.webcom.com/~haahr/
// -------------------------------------------------------

/* This will be optimized by the compiler */
unsigned long PdfAscii85Filter::sPowers85[] = {
    85*85*85*85, 85*85*85, 85*85, 85, 1
};

void PdfAscii85Filter::EncodeTuple( unsigned long tuple, int count )
{
    int      i      = 5;
    int      z      = 0;
    char     buf[5];
    char     out[5];
    char*    start  = buf;;

    do 
    {
        *start++ = tuple % 85;
        tuple /= 85;
    } 
    while (--i > 0);
    
    i = count;
    do 
    {
        out[z++] = static_cast<unsigned char>(*--start) + '!';
    } 
    while (i-- > 0);

    m_pOutputStream->Write( out, z );
}

void PdfAscii85Filter::BeginEncode( PdfOutputStream* pOutput )
{
    if( m_pOutputStream ) 
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode has already an output stream. Did you forget to call EndEncode()?" );
    }

    m_count = 0;
    m_tuple = 0;
    m_pOutputStream = pOutput;
}

void PdfAscii85Filter::EncodeBlock( const char* pBuffer, long lLen )
{
    unsigned int  c;
    const char*   z = "z";

    if( !m_pOutputStream )
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode was not yet called or EndEncode was called before this method.");
    }

    while( lLen ) 
    {
        c = *pBuffer & 0xff;
	switch (m_count++) {
            case 0: m_tuple |= ( c << 24); break;
            case 1: m_tuple |= ( c << 16); break;
            case 2: m_tuple |= ( c <<  8); break;
            case 3:
		m_tuple |= c;
		if( 0 == m_tuple ) 
                {
                    m_pOutputStream->Write( z, 1 );
		} 
                else
                {
                    this->EncodeTuple( m_tuple, m_count ); 
                }

		m_tuple = 0;
		m_count = 0;
		break;
	}
        --lLen;
        ++pBuffer;
    }
}

void PdfAscii85Filter::EndEncode()
{
    if( !m_pOutputStream ) 
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode was not yet called or EndEncode was called before this method.");
    }

    if( m_count > 0 )
        this->EncodeTuple( m_tuple, m_count );

    m_pOutputStream = NULL;
}

void PdfAscii85Filter::Decode( const char* pInBuffer, long lInLen, char** ppOutBuffer, long* plOutLen, const PdfDictionary* ) const
{
    unsigned long tuple = 0;
    int           count = 0;
    int           pos   = 0;

    if( !plOutLen || !pInBuffer || !ppOutBuffer )
    {
        RAISE_ERROR( ePdfError_InvalidHandle );
    }

    *plOutLen    = lInLen;
    *ppOutBuffer = static_cast<char*>(malloc( *plOutLen * sizeof(char) ));
   
    if( !*ppOutBuffer )
    {
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    --lInLen;
    while( lInLen ) 
    {
        switch ( *pInBuffer ) 
        {
            default:
                if ( *pInBuffer < '!' || *pInBuffer > 'u') 
                {
                    RAISE_ERROR( ePdfError_ValueOutOfRange );
                }

                tuple += ( *pInBuffer - '!') * PdfAscii85Filter::sPowers85[count++];
                if (count == 5) 
                {
                    WidePut( *ppOutBuffer, &pos, *plOutLen, tuple, 4 );
                    count = 0;
                    tuple = 0;
                }
                break;
            case 'z':
                if (count != 0 ) 
                {
                    RAISE_ERROR( ePdfError_ValueOutOfRange );
                }

                if( pos + 4 >= *plOutLen )
                {
                    RAISE_ERROR( ePdfError_OutOfMemory );
                }

                (*ppOutBuffer)[ pos++ ] = 0;
                (*ppOutBuffer)[ pos++ ] = 0;
                (*ppOutBuffer)[ pos++ ] = 0;
                (*ppOutBuffer)[ pos++ ] = 0;
                break;
            case '~':
                ++pInBuffer; 
                if( *pInBuffer != '>' ) 
                {
                    RAISE_ERROR( ePdfError_ValueOutOfRange );
                }

                break;
            case '\n': case '\r': case '\t': case ' ':
            case '\0': case '\f': case '\b': case 0177:
                break;
        }

        --lInLen;
        ++pInBuffer;
    }

    if (count > 0) 
    {
        count--;
        tuple += PdfAscii85Filter::sPowers85[count];
        WidePut( *ppOutBuffer, &pos, *plOutLen, tuple, count );
    }

    *plOutLen = pos;
}

void PdfAscii85Filter::WidePut( char* pBuffer, int* bufferPos, long lBufferLen, unsigned long tuple, int bytes ) const
{
    if( *bufferPos + bytes >= lBufferLen ) 
    {
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    switch (bytes) {
	case 4:
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 24);
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 16);
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >>  8);
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple);
            break;
	case 3:
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 24);
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 16);
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >>  8);
            break;
	case 2:
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 24);
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 16);
            break;
	case 1:
            pBuffer[ (*bufferPos)++ ] = static_cast<char>(tuple >> 24);
            break;
    }
}

// -------------------------------------------------------
// Flate
// -------------------------------------------------------
void PdfFlateFilter::BeginEncode( PdfOutputStream* pOutput )
{
    if( m_pOutputStream ) 
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode has already an output stream. Did you forget to call EndEncode()?" );
    }

    m_stream.zalloc   = Z_NULL;
    m_stream.zfree    = Z_NULL;
    m_stream.opaque   = Z_NULL;
    
    if( deflateInit( &m_stream, Z_DEFAULT_COMPRESSION ) )
    {
        RAISE_ERROR( ePdfError_Flate );
    }

    m_pOutputStream = pOutput;
}

void PdfFlateFilter::EncodeBlock( const char* pBuffer, long lLen )
{
    if( !m_pOutputStream )
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "BeginEncode was not yet called or EndEncode was called before this method.");
    }

    this->EncodeBlockInternal( pBuffer, lLen, Z_NO_FLUSH );
}

void PdfFlateFilter::EncodeBlockInternal( const char* pBuffer, long lLen, int nMode )
{
    int nWrittenData;

    m_stream.avail_in = lLen;
    m_stream.next_in  = (Bytef*)(pBuffer);

    do {
        m_stream.avail_out = FILTER_INTERNAL_BUFFER_SIZE;
        m_stream.next_out = m_buffer;

        if( deflate( &m_stream, nMode) == Z_STREAM_ERROR )
        {
            m_pOutputStream = NULL;
            RAISE_ERROR( ePdfError_Flate );
        }


        nWrittenData = FILTER_INTERNAL_BUFFER_SIZE - m_stream.avail_out;
        try {
            m_pOutputStream->Write( (const char*)(m_buffer), nWrittenData );
        } catch( PdfError & e ) {
            // clean up after any output stream errors
            m_pOutputStream = NULL;
            
            e.AddToCallstack( __FILE__, __LINE__ );
            throw e;
        }
    } while( m_stream.avail_out == 0 );
}

void PdfFlateFilter::EndEncode()
{
    if( !m_pOutputStream )
    {
        RAISE_ERROR_INFO( ePdfError_InternalLogic, 
                          "Call BeginEncode before calling EndEncode()!.");
    }

    this->EncodeBlockInternal( NULL, 0, Z_FINISH );
    
    deflateEnd( &m_stream );

    m_pOutputStream = NULL;
}


void PdfFlateFilter::Decode( const char* pInBuffer, long lInLen, char** ppOutBuffer, long* plOutLen, const PdfDictionary* pDecodeParms ) const
{
    int          flateErr;
    unsigned int have;
    z_stream strm;
    char  out[CHUNK];
    char* pBuf = NULL;
    char* pTmp = NULL;

    long  lBufSize = 0;
    TFlatePredictorParams tParams;

    if( !pInBuffer || !plOutLen || !ppOutBuffer )
    {
        RAISE_ERROR( ePdfError_InvalidHandle );
    }

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    if( inflateInit(&strm) != Z_OK)
    {
        RAISE_ERROR( ePdfError_Flate );
    }

    strm.avail_in = lInLen;
    strm.next_in  = (Bytef*)(pInBuffer);

    do {
        strm.avail_out = CHUNK;
        strm.next_out  = (Bytef*)(out);

        switch ( (flateErr = inflate(&strm, Z_NO_FLUSH)) ) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            {
                PdfError::LogMessage( eLogSeverity_Error, "Flate Decoding Error from ZLib: %i\n", flateErr );
                (void)inflateEnd(&strm);

                RAISE_ERROR( ePdfError_Flate );
            }
            default:
                break;
        }

        have = CHUNK - strm.avail_out;

        if( pBuf )
            pBuf = static_cast<char*>(realloc( pBuf, sizeof( char ) * (lBufSize + have) ));
        else
            pBuf = static_cast<char*>(malloc( sizeof( char ) * (lBufSize + have) ));

        if( !pBuf )
        {
            (void)inflateEnd(&strm);
            free( pTmp );
            RAISE_ERROR( ePdfError_InvalidHandle );
        }

        memcpy( (pBuf+lBufSize), out, have );
        lBufSize += have;
        free( pTmp );
    } while (strm.avail_out == 0);
    
    /* clean up and return */
    (void)inflateEnd(&strm);

    *ppOutBuffer = pBuf;
    *plOutLen    = lBufSize;

    if( pDecodeParms ) 
    {
        tParams.nPredictor   = pDecodeParms->GetKeyAsLong( "Predictor", tParams.nPredictor );
        tParams.nColors      = pDecodeParms->GetKeyAsLong( "Colors", tParams.nColors );
        tParams.nBPC         = pDecodeParms->GetKeyAsLong( "BitsPerComponent", tParams.nBPC );
        tParams.nColumns     = pDecodeParms->GetKeyAsLong( "Columns", tParams.nColumns );
        tParams.nEarlyChange = pDecodeParms->GetKeyAsLong( "EarlyChange", tParams.nEarlyChange );

        try {
            this->RevertPredictor( &tParams, pBuf, lBufSize, ppOutBuffer, plOutLen );
        } catch( PdfError & e ) {
            free( pBuf );
            e.AddToCallstack( __FILE__, __LINE__ );
            throw e;
        }

        free( pBuf );
    }
}

// -------------------------------------------------------
// Flate Predictor
// -------------------------------------------------------
void PdfFlateFilter::RevertPredictor( const TFlatePredictorParams* pParams, const char* pInBuffer, long lInLen, char** ppOutBuffer, long* plOutLen ) const
{
    unsigned char*   pPrev;
    int     nRows;
    int     i;
    char*   pOutBufStart;
    const char*   pBuffer = pInBuffer;
    int     nPredictor;

#ifdef PODOFO_VERBOSE_DEBUG
    PdfError::DebugMessage("Applying Predictor %i to buffer of size %i\n", pParams->nPredictor, lInLen );
    PdfError::DebugMessage("Cols: %i Modulo: %i Comps: %i\n", pParams->nColumns, lInLen % (pParams->nColumns +1), pParams->nBPC );
#endif // PODOFO_VERBOSE_DEBUG

    if( pParams->nPredictor == 1 )  // No Predictor
        return;

    nRows = (pParams->nColumns * pParams->nBPC) >> 3; 

    pPrev = static_cast<unsigned char*>(malloc( sizeof(char) * nRows ));
    if( !pPrev )
    {
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    memset( pPrev, 0, sizeof(char) * nRows );

#ifdef PODOFO_VERBOSE_DEBUG
    PdfError::DebugMessage("Alloc: %i\n", (lInLen / (pParams->nColumns + 1)) * pParams->nColumns );
#endif // PODOFO_VERBOSE_DEBUG

    *ppOutBuffer = static_cast<char*>(malloc( sizeof(char) * (lInLen / (pParams->nColumns + 1)) * pParams->nColumns ));
    pOutBufStart = *ppOutBuffer;

    if( !*ppOutBuffer )
    {
        free( pPrev );
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    while( pBuffer < (pInBuffer + lInLen) )
    {
        nPredictor = pParams->nPredictor >= 10 ? *pBuffer + 10 : *pBuffer;
        ++pBuffer;

        for( i=0;i<nRows;i++ )
        {
            switch( nPredictor )
            {
                case 2: // Tiff Predictor
                    // TODO: implement tiff predictor
                    
                    break;
                case 10: // png none
                case 11: // png sub
                case 12: // png up
                    *pOutBufStart = static_cast<unsigned char>(pPrev[i] + static_cast<unsigned char>(*pBuffer));
                    break;
                case 13: // png average
                case 14: // png paeth
                case 15: // png optimum
                    break;
                
                default:
                {
                    free( pPrev );
                    RAISE_ERROR( ePdfError_InvalidPredictor );
                    break;
                }
            }
  
            pPrev[i] = *pOutBufStart;          
            ++pOutBufStart;
            ++pBuffer;
        }
    }

    *plOutLen = (pOutBufStart - *ppOutBuffer);

    free( pPrev );
}

// -------------------------------------------------------
// RLE
// -------------------------------------------------------

void PdfRLEFilter::BeginEncode( PdfOutputStream* )
{
    RAISE_ERROR( ePdfError_UnsupportedFilter );
}

void PdfRLEFilter::EncodeBlock( const char*, long )
{
    RAISE_ERROR( ePdfError_UnsupportedFilter );
}

void PdfRLEFilter::EndEncode()
{
    RAISE_ERROR( ePdfError_UnsupportedFilter );
}

void PdfRLEFilter::Decode( const char* pInBuffer, long lInLen, char** ppOutBuffer, long* plOutLen, const PdfDictionary* ) const
{
    char*                 pBuf;
    long                  lCur;
    long                  lSize;
    unsigned char         cLen;
    int                   i;

    if( !plOutLen || !pInBuffer || !ppOutBuffer )
    {
        RAISE_ERROR( ePdfError_InvalidHandle );
    }

    lCur  = 0;
    lSize = lInLen;
    pBuf  = static_cast<char*>(malloc( sizeof(char)*lSize ));
    if( !pBuf )
    {
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    while( lInLen )
    {
        cLen = *pInBuffer;
        ++pInBuffer;

        if( cLen == 128 )
            // reached EOD
            break;
        else if( cLen <= 127 )
        {
            if( lCur + cLen+1 > lSize )
            {
                // buffer to small, do a realloc
                lSize = PDF_MAX( lCur + cLen+1, lSize << 1 );
                pBuf  = static_cast<char*>(realloc( pBuf, lSize  ));
                if( !pBuf )
                {
                    RAISE_ERROR( ePdfError_OutOfMemory );
                }
            }
                
            memcpy( pBuf + lCur, pInBuffer, cLen+1 );
            lCur      += (cLen + 1);
            pInBuffer += (cLen + 1);
            lInLen    -= (cLen + 1);
        }
        else if( cLen >= 129 )
        {
            cLen = 257 - cLen;

            if( lCur + cLen > lSize )
            {
                // buffer to small, do a realloc
                lSize = PDF_MAX( lCur + cLen, lSize << 1 );
                pBuf  = static_cast<char*>(realloc( pBuf, lSize ));
                if( !pBuf )
                {
                    RAISE_ERROR( ePdfError_OutOfMemory );
                }
            }

            for( i=0;i<cLen;i++ )
            {
                *(pBuf + lCur) = *pInBuffer;
                ++lCur;
            }

            ++pInBuffer;
            --lInLen;
        }
    }

    *ppOutBuffer = pBuf;
    *plOutLen    = lCur;
}

// -------------------------------------------------------
// RLE
// -------------------------------------------------------

const unsigned short PdfLZWFilter::s_masks[] = { 0x01FF,
                                        0x03FF,
                                        0x07FF,
                                        0x0FFF };

const unsigned short PdfLZWFilter::s_clear  = 0x0100;      // clear table
const unsigned short PdfLZWFilter::s_eod    = 0x0101;      // end of data

void PdfLZWFilter::BeginEncode( PdfOutputStream* )
{
    RAISE_ERROR( ePdfError_UnsupportedFilter );
}

void PdfLZWFilter::EncodeBlock( const char*, long )
{
    RAISE_ERROR( ePdfError_UnsupportedFilter );
}

void PdfLZWFilter::EndEncode()
{
    RAISE_ERROR( ePdfError_UnsupportedFilter );
}

void PdfLZWFilter::Decode( const char* pInBuffer, long lInLen, char** ppOutBuffer, long* plOutLen, const PdfDictionary* pDecodeParms ) const
{
    TLzwTable table( LZW_TABLE_SIZE ); // the lzw table;
    TLzwItem  item;

    std::vector<unsigned char> output;
    std::vector<unsigned char> data;
    std::vector<unsigned char>::const_iterator it;

    unsigned int mask             = 0;
    unsigned int code_len         = 9;
    unsigned char character       = 0;

    pdf_uint32   old              = 0;
    pdf_uint32   code             = 0;
    pdf_uint32   buffer           = 0;

    unsigned int buffer_size      = 0;
    const unsigned int buffer_max = 24;
    
    *ppOutBuffer = NULL;
    *plOutLen    = 0;

    InitTable( &table );

    if( lInLen )
        character = *pInBuffer;

    while( lInLen > 0 ) 
    {
        // Fill the buffer
        while( buffer_size <= (buffer_max-8) )
        {
            buffer <<= 8;
            buffer |= static_cast<pdf_uint32>(static_cast<unsigned char>(*pInBuffer));
            buffer_size += 8;

            ++pInBuffer;
            lInLen--;
        }

        // read from the buffer
        while( buffer_size >= code_len ) 
        {
            code = (buffer >> (buffer_size - code_len)) & PdfLZWFilter::s_masks[mask];
            buffer_size -= code_len;

            if( code == PdfLZWFilter::s_clear ) 
            {
                mask     = 0;
                code_len = 9;

                InitTable( &table );
            }
            else if( code == PdfLZWFilter::s_eod ) 
            {
                lInLen = 0;
                break;
            }
            else 
            {
                if( code >= table.size() )
                {
                    if (old >= table.size())
                    {
                        RAISE_ERROR( ePdfError_ValueOutOfRange );
                    }
                    data = table[old].value;
                    data.push_back( character );
                }
                else
                    data = table[code].value;

                it = data.begin();
                while( it != data.end() )
                {
                    output.push_back( *it );
                    ++it;
                }

                character = data[0];
                if( old < table.size() ) // fix the first loop
                    data = table[old].value;
                data.push_back( character );

                item.value = data;
                table.push_back( item );

                old = code;

                switch( table.size() ) 
                {
                    case 511:
                    case 1023:
                    case 2047:
                    case 4095:
                        ++code_len;
                        ++mask;
                    default:
                        break;
                }
            }
        }
    }

    *ppOutBuffer = static_cast<char*>(malloc( sizeof(char) * output.size() ));
    if( !*ppOutBuffer ) 
    {
        RAISE_ERROR( ePdfError_OutOfMemory );
    }

    *plOutLen = output.size();

    memcpy( *ppOutBuffer, &(output[0]), *plOutLen );
}

void PdfLZWFilter::InitTable( TLzwTable* pTable ) const
{
    int      i;
    TLzwItem item;

    pTable->clear();
    pTable->reserve( LZW_TABLE_SIZE );

    for( i=0;i<255;i++ )
    {
        item.value.clear();
        item.value.push_back( (unsigned char)i );
        pTable->push_back( item );
    }

    item.value.clear();
    item.value.push_back( (unsigned char)s_clear );
    pTable->push_back( item );

    item.value.clear();
    item.value.push_back( (unsigned char)s_clear );
    pTable->push_back( item );
}

};
