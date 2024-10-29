#pragma once

#include "flexdmd_i.h"

#include "AssetSrc.h"
#include <SDL3_image/SDL_image.h>

class Bitmap : public IDispatch
{
public:
   STDMETHOD(QueryInterface)(REFIID iid, void** ppv) {
      if (iid == IID_IDispatch) {
         *ppv = reinterpret_cast<Bitmap*>(this);
         AddRef();
         return S_OK;
      } else if (iid == IID_IUnknown) {
         *ppv = reinterpret_cast<Bitmap*>(this);
         AddRef();
         return S_OK;
      } else {
         *ppv = NULL;
         return E_NOINTERFACE;
      }
   }

   STDMETHOD_(ULONG, AddRef)() {
      m_dwRef++;
      return m_dwRef;
   }

   STDMETHOD_(ULONG, Release)() {
      ULONG dwRef = --m_dwRef;

      if (dwRef == 0)
         delete this;

      return dwRef;
   }

   STDMETHOD(GetTypeInfoCount)(UINT *pCountTypeInfo) { *pCountTypeInfo = 0; return S_OK;  }
   STDMETHOD(GetTypeInfo)(UINT iInfo, LCID lcid, ITypeInfo **ppTInfo) { return E_NOTIMPL; }
   STDMETHOD(GetIDsOfNames)(REFIID /*riid*/, LPOLESTR* rgszNames, UINT cNames, LCID lcid,DISPID* rgDispId) { return E_NOTIMPL; }
   STDMETHOD(Invoke)(DISPID dispIdMember, REFIID /*riid*/, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) { return E_NOTIMPL; }

public:
   Bitmap(void* pData, AssetType assetType);
   Bitmap(Bitmap* pCachedBitmap);
   ~Bitmap();

   int GetWidth();
   int GetHeight();
   float GetLength();
   float GetFrameDelay(int pos);
   int GetFrameCount();
   SDL_Surface* GetSurface();
   SDL_Surface* GetFrameSurface(int pos);
   void SetData(void* pData);

private:
   AssetType m_assetType;
   void* m_pData;

   ULONG m_dwRef = 0;
};