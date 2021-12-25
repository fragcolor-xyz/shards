using System.Runtime.InteropServices;
using System;
using UnityEngine;
using Unity.Collections.LowLevel.Unsafe;

namespace Chainblocks
{
  [StructLayout(LayoutKind.Sequential)]
  public struct CBlock
  {
    //! Native struct, don't edit

    IntPtr _inlineBlockId;
    IntPtr _owned;
    IntPtr _name;
    IntPtr _hash;
    IntPtr _help;
    IntPtr _inputHelp;
    IntPtr _outputHelp;
    IntPtr _properties;
    IntPtr _setup;
    IntPtr _destroy;
    IntPtr _inputTypes;
    IntPtr _outputTypes;
    IntPtr _exposedVariables;
    IntPtr _requiredVariables;
    IntPtr _compose;
    IntPtr _composed;
    IntPtr _parameters;
    IntPtr _setParam;
    IntPtr _getParam;
    IntPtr _warmup;
    IntPtr _activate;
    IntPtr _cleanup;
    IntPtr _nextFrame;
    IntPtr _mutate;
    IntPtr _crossover;
    IntPtr _getState;
    IntPtr _setState;
    IntPtr _resetState;
  }

  [StructLayout(LayoutKind.Explicit, Size = 32)]
  public struct CBVar                  // 2048 bytes in header
  {
    //! Native struct, don't edit

    [FieldOffset(0)]
    public Vector3 vector3;

    [FieldOffset(0)]
    public IntPtr chainRef;

    [FieldOffset(28)]
    public byte type;

    [FieldOffset(29)]
    public byte innerType;

    [FieldOffset(30)]
    public byte flags;

    [FieldOffset(31)]
    public byte _unusedByte;
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct CBCore
  {
    //! Native struct, don't edit

    internal IntPtr _alloc;
    internal IntPtr _free;
    internal IntPtr _tableNew;
    internal IntPtr _setNew;
    internal IntPtr _composeBlocks;
    internal IntPtr _runBlocks;
    internal IntPtr _runBlocks2;
    internal IntPtr _runBlocksHashed;
    internal IntPtr _runBlocksHashed2;
    internal IntPtr _log;
    internal IntPtr _logLevel;
    internal IntPtr _createBlock;
    internal IntPtr _validateSetParam;
    internal IntPtr _createChain;
    internal IntPtr _setChainName;
    internal IntPtr _setChainLooped;
    internal IntPtr _setChainUnsafe;
    internal IntPtr _addBlock;
    internal IntPtr _removeBlock;
    internal IntPtr _destroyChain;
    internal IntPtr _stopChain;
    internal IntPtr _composeChain;
    internal IntPtr _runChain;
    internal IntPtr _getChainInfo;
    internal IntPtr _getGlobalChain;
    internal IntPtr _setGlobalChain;
    internal IntPtr _unsetGlobalChain;
    internal IntPtr _createNode;
    internal IntPtr _destroyNode;
    internal IntPtr _schedule;
    internal IntPtr _unschedule;
    internal IntPtr _tick;
    internal IntPtr _sleep;
    internal IntPtr _getRootPath;
    internal IntPtr _setRootPath;
    internal IntPtr _asyncActivate;
    internal IntPtr _getBlocks;
    internal IntPtr _registerBlock;
    internal IntPtr _registerObjectType;
    internal IntPtr _registerEnumType;
    internal IntPtr _registerRunLoopCallback;
    internal IntPtr _unregisterRunLoopCallback;
    internal IntPtr _registerExitCallback;
    internal IntPtr _unregisterExitCallback;
    internal IntPtr _referenceVariable;
    internal IntPtr _referenceChainVariable;
    internal IntPtr _releaseVariable;
    internal IntPtr _setExternalVariable;
    internal IntPtr _removeExternalVariable;
    internal IntPtr _allocExternalVariable;
    internal IntPtr _freeExternalVariable;
    internal IntPtr _suspend;
    internal IntPtr _getState;
    internal IntPtr _abortChain;
    internal IntPtr _cloneVar;
    internal IntPtr _destroyVar;
    internal IntPtr _readCachedString;
    internal IntPtr _writeCachedString;
    internal IntPtr _isEqualVar;
    internal IntPtr _isEqualType;
    internal IntPtr _deriveTypeInfo;
    internal IntPtr _freeDerivedTypeInfo;
    internal IntPtr _seqFree;
    internal IntPtr _seqPush;
    internal IntPtr _seqInsert;
    internal IntPtr _seqPop;
    internal IntPtr _seqResize;
    internal IntPtr _seqFastDelete;
    internal IntPtr _seqSlowDelete;
    internal IntPtr _typesFree;
    internal IntPtr _typesPush;
    internal IntPtr _typesInsert;
    internal IntPtr _typesPop;
    internal IntPtr _typesResize;
    internal IntPtr _typesFastDelete;
    internal IntPtr _typesSlowDelete;
    internal IntPtr _paramsFree;
    internal IntPtr _paramsPush;
    internal IntPtr _paramsInsert;
    internal IntPtr _paramsPop;
    internal IntPtr _paramsResize;
    internal IntPtr _paramsFastDelete;
    internal IntPtr _paramsSlowDelete;
    internal IntPtr _blocksFree;
    internal IntPtr _blocksPush;
    internal IntPtr _blocksInsert;
    internal IntPtr _blocksPop;
    internal IntPtr _blocksResize;
    internal IntPtr _blocksFastDelete;
    internal IntPtr _blocksSlowDelete;
    internal IntPtr _expTypesFree;
    internal IntPtr _expTypesPush;
    internal IntPtr _expTypesInsert;
    internal IntPtr _expTypesPop;
    internal IntPtr _expTypesResize;
    internal IntPtr _expTypesFastDelete;
    internal IntPtr _expTypesSlowDelete;
    internal IntPtr _enumsFree;
    internal IntPtr _enumsPush;
    internal IntPtr _enumsInsert;
    internal IntPtr _enumsPop;
    internal IntPtr _enumsResize;
    internal IntPtr _enumsFastDelete;
    internal IntPtr _enumsSlowDelete;
    internal IntPtr _stringsFree;
    internal IntPtr _stringsPush;
    internal IntPtr _stringsInsert;
    internal IntPtr _stringsPop;
    internal IntPtr _stringsResize;
    internal IntPtr _stringsFastDelete;
    internal IntPtr _stringsSlowDelete;
  }

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr AllocDelegate(UInt32 size);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate void FreeDelegate(IntPtr ptr);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate byte SuspendDelegate(IntPtr context, double duration);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr AllocExternalVariableDelegate(IntPtr chain, string name);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate void FreeExternalVariableDelegate(IntPtr chain, string name);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr CreateNodeDelegate();

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate byte TickDelegate(IntPtr nodeRef);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate void ScheduleDelegate(IntPtr nodeRef, IntPtr chainRef);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate void DestroyVarDelegate(IntPtr varRef);

  public static class CBCoreExt
  {
    static public IntPtr Alloc(this CBCore core, UInt32 size)
    {
      var allocDelegate = Marshal.GetDelegateForFunctionPointer<AllocDelegate>(core._alloc);
      return allocDelegate(size);
    }

    static public void Free(this CBCore core, IntPtr ptr)
    {
      var freeDelegate = Marshal.GetDelegateForFunctionPointer<FreeDelegate>(core._free);
      freeDelegate(ptr);
    }

    static public byte Suspend(this CBCore core, IntPtr context, double duration)
    {
      var suspendDelegate = Marshal.GetDelegateForFunctionPointer<SuspendDelegate>(core._suspend);
      return suspendDelegate(context, duration);
    }

    static public IntPtr AllocExternalVariable(this CBCore core, IntPtr chain, string name)
    {
      var allocExternalVariableDelegate = Marshal.GetDelegateForFunctionPointer<AllocExternalVariableDelegate>(core._allocExternalVariable);
      return allocExternalVariableDelegate(chain, name);
    }

    static public void FreeExternalVariable(this CBCore core, IntPtr chain, string name)
    {
      var freeExternalVariableDelegate = Marshal.GetDelegateForFunctionPointer<FreeExternalVariableDelegate>(core._freeExternalVariable);
      freeExternalVariableDelegate(chain, name);
    }

    static public IntPtr CreateNode(this CBCore core)
    {
      var createNodeDelegate = Marshal.GetDelegateForFunctionPointer<CreateNodeDelegate>(core._createNode);
      return createNodeDelegate();
    }

    static public byte Tick(this CBCore core, IntPtr nodeRef)
    {
      var tickDelegate = Marshal.GetDelegateForFunctionPointer<TickDelegate>(core._tick);
      return tickDelegate(nodeRef);
    }

    static public void Schedule(this CBCore core, IntPtr nodeRef, IntPtr chainRef)
    {
      var scheduleDelegate = Marshal.GetDelegateForFunctionPointer<ScheduleDelegate>(core._schedule);
      scheduleDelegate(nodeRef, chainRef);
    }

    static public void DestroyVar(this CBCore core, IntPtr varRef)
    {
      var destroyVarDelegate = Marshal.GetDelegateForFunctionPointer<DestroyVarDelegate>(core._destroyVar);
      destroyVarDelegate(varRef);
    }
  }

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  public delegate CBVar ActivateUnmanagedDelegate(IntPtr context, IntPtr input);

  public static class Native
  {
    static IntPtr _core;
    static CBCore _coreCopy;

    const string Dll = "libcbl";
    const CallingConvention Conv = CallingConvention.Cdecl;

    [DllImport(Native.Dll, CallingConvention = Native.Conv)]
    public static extern IntPtr chainblocksInterface(Int32 version);

    [DllImport(Native.Dll, CallingConvention = Native.Conv)]
    public static extern IntPtr cbLispCreate(String path);

    [DllImport(Native.Dll, CallingConvention = Native.Conv)]
    public static extern void cbLispDestroy(IntPtr lisp);

    [DllImport(Native.Dll, CallingConvention = Native.Conv)]
    public static extern byte cbLispEval(IntPtr lisp, String code, IntPtr output);

    public static ref CBCore Core
    {
      get
      {
        if (_core == IntPtr.Zero)
        {
          _core = chainblocksInterface(0x20200101);
          _coreCopy = Marshal.PtrToStructure<CBCore>(_core);
        }
        return ref _coreCopy;
      }
    }
  }

  public class ExternalVariable : IDisposable
  {
    IntPtr _var;
    string _name;
    IntPtr _chain;

    public ExternalVariable(IntPtr chain, string name)
    {
      _chain = chain;
      _name = name;
      _var = Native.Core.AllocExternalVariable(chain, _name);
    }

    public void Dispose()
    {
      Native.Core.FreeExternalVariable(_chain, _name);
    }

    public ref CBVar Value
    {
      get
      {
        unsafe
        {
          return ref UnsafeUtility.AsRef<CBVar>(_var.ToPointer());
        }
      }
    }
  }

  public class Variable : IDisposable
  {
    IntPtr _mem;
    bool _destroy;

    public Variable(bool destroy = true)
    {
      _destroy = destroy;
      _mem = Native.Core.Alloc(32);
    }

    public void Dispose()
    {
      if (_destroy)
      {
        Native.Core.DestroyVar(_mem);
      }
      Native.Core.Free(_mem);
    }

    public ref CBVar Value
    {
      get
      {
        unsafe
        {
          return ref UnsafeUtility.AsRef<CBVar>(_mem.ToPointer());
        }
      }
    }

    public IntPtr Ptr
    {
      get
      {
        return _mem;
      }
    }
  }
}
