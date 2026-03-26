#include "panini.h"

void SaveD3D9State(IDirect3DDevice9* dev, D3D9State* s) {
    dev->GetPixelShader(&s->pPixelShader);
    dev->GetVertexShader(&s->pVertexShader);
    dev->GetRenderTarget(0, &s->pRenderTarget0);
    dev->GetTexture(0, &s->pTexture0);
    dev->GetFVF(&s->fvf);
    dev->GetRenderState(D3DRS_ZENABLE, &s->zenable);
    dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &s->alphablend);
    dev->GetRenderState(D3DRS_CULLMODE, &s->cullmode);
    dev->GetRenderState(D3DRS_STENCILENABLE, &s->stencilenable);
    dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &s->scissortest);
    dev->GetRenderState(D3DRS_COLORWRITEENABLE, &s->colorwrite);
    dev->GetTextureStageState(0, D3DTSS_COLOROP, &s->tss_colorop);
    dev->GetTextureStageState(0, D3DTSS_ALPHAOP, &s->tss_alphaop);
    dev->GetSamplerState(0, D3DSAMP_MINFILTER, &s->samp_minfilter);
    dev->GetSamplerState(0, D3DSAMP_MAGFILTER, &s->samp_magfilter);
    dev->GetSamplerState(0, D3DSAMP_ADDRESSU, &s->samp_addressu);
    dev->GetSamplerState(0, D3DSAMP_ADDRESSV, &s->samp_addressv);
    dev->GetViewport(&s->viewport);
}

void RestoreD3D9State(IDirect3DDevice9* dev, const D3D9State* s) {
    dev->SetPixelShader(s->pPixelShader);
    dev->SetVertexShader(s->pVertexShader);
    dev->SetRenderTarget(0, s->pRenderTarget0);
    dev->SetTexture(0, s->pTexture0);
    dev->SetFVF(s->fvf);
    dev->SetRenderState(D3DRS_ZENABLE, s->zenable);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, s->alphablend);
    dev->SetRenderState(D3DRS_CULLMODE, s->cullmode);
    dev->SetRenderState(D3DRS_STENCILENABLE, s->stencilenable);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, s->scissortest);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, s->colorwrite);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, s->tss_colorop);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, s->tss_alphaop);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, s->samp_minfilter);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, s->samp_magfilter);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, s->samp_addressu);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, s->samp_addressv);
    dev->SetViewport(&s->viewport);

    // Release COM references acquired by Get* calls.
    if (s->pPixelShader) s->pPixelShader->Release();
    if (s->pVertexShader) s->pVertexShader->Release();
    if (s->pRenderTarget0) s->pRenderTarget0->Release();
    if (s->pTexture0) s->pTexture0->Release();
}
