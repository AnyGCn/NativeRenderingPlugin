using System;
using UnityEngine;
using UnityEngine.Rendering;

public class MetalFXTest : MonoBehaviour
{
    private RenderTexture src;
    private RenderTexture dst;
    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        src = new RenderTexture(512, 512, 0, RenderTextureFormat.ARGB32)
        {
            name = "MetalFX src RT",
        };
        src.Create();
        dst = new RenderTexture(1024, 1024, 0, RenderTextureFormat.ARGB32)
        {
            name = "MetalFX dst RT",
        };
        
        dst.Create();
    }

    // Update is called once per frame
    void Update()
    {
        // CommandBuffer cmd = new CommandBuffer();
        // cmd.name = "MetalFX Upscale";
        // cmd.SetRenderTarget(src);
        // cmd.ClearRenderTarget(false, true, Color.blue);
        // RenderingPluginWrapper.UpscaleTexture(cmd, src, dst);
        // // cmd.Blit(temporary, RenderTexture.active);
        // Graphics.ExecuteCommandBuffer(cmd);
        // cmd.Release();
    }

    public void OnDestroy()
    {
        GameObject.Destroy(src);
        GameObject.Destroy(dst);
    }
    
    // OnRenderImage is called after all rendering is complete to render image
    private void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        RenderTexture temporary = RenderTexture.GetTemporary(source.width, source.height, 0, source.graphicsFormat);
        temporary.name = "MetalFX Temp RT";
        if (!temporary.IsCreated()) temporary.Create();
        CommandBuffer cmd = new CommandBuffer();
        cmd.name = "MetalFX Upscale";
        RenderingPluginWrapper.UpscaleTexture(cmd, source, temporary);
        cmd.Blit(temporary, destination);
        Graphics.ExecuteCommandBuffer(cmd);
        cmd.Release();
        RenderTexture.ReleaseTemporary(temporary);
    }
}
