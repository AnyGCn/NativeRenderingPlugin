using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

public static class RenderingPluginWrapper
{
    public enum RenderEvent
    {
        UpscaleTexture = 0,
    }
    
    [StructLayout(LayoutKind.Sequential)]
    private struct UpscaleTextureData
    {
        public IntPtr input;
        public IntPtr output;
    }
    
    public static unsafe void UpscaleTexture(this CommandBuffer cmd, RenderTexture input, RenderTexture output)
    {
        var data = new UpscaleTextureData
        {
            input = input.GetNativeTexturePtr(),
            output = output.GetNativeTexturePtr()
        };
        
        GCHandle gcHandle = GCHandle.Alloc(data, GCHandleType.Pinned);
        cmd.IssuePluginEventAndData(GetRenderEventWithDataFunc(), (int)RenderEvent.UpscaleTexture, gcHandle.AddrOfPinnedObject());
        gcHandle.Free();
    }
    
#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
    [DllImport("RenderingPlugin")]
#endif
    private static extern IntPtr GetRenderEventFunc();

#if (PLATFORM_IOS || PLATFORM_TVOS || PLATFORM_BRATWURST || PLATFORM_SWITCH) && !UNITY_EDITOR
    [DllImport("__Internal")]
#else
    [DllImport("RenderingPlugin")]
#endif
    private static extern IntPtr GetRenderEventWithDataFunc();
}
