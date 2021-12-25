using System.Runtime.InteropServices;
using System;
using UnityEngine;

namespace Chainblocks
{
  [StructLayout(LayoutKind.Sequential)]
  public struct CBlock
  {
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
    IntPtr _tableNew;
    IntPtr _setNew;
    IntPtr _composeBlocks;
    IntPtr _runBlocks;
    IntPtr _runBlocks2;
    IntPtr _runBlocksHashed;
    IntPtr _runBlocksHashed2;
    IntPtr _log;
    IntPtr _logLevel;
    IntPtr _createBlock;
    IntPtr _validateSetParam;
    IntPtr _createChain;
    IntPtr _setChainName;
    IntPtr _setChainLooped;
    IntPtr _setChainUnsafe;
    IntPtr _addBlock;
    IntPtr _removeBlock;
    IntPtr _destroyChain;
    IntPtr _stopChain;
    IntPtr _composeChain;
    IntPtr _runChain;
    IntPtr _getChainInfo;
    IntPtr _getGlobalChain;
    IntPtr _setGlobalChain;
    IntPtr _unsetGlobalChain;
    IntPtr _createNode;
    IntPtr _destroyNode;
    IntPtr _schedule;
    IntPtr _unschedule;
    IntPtr _tick;
    IntPtr _sleep;
    IntPtr _getRootPath;
    IntPtr _setRootPath;
    IntPtr _asyncActivate;
    IntPtr _getBlocks;
    IntPtr _registerBlock;
    IntPtr _registerObjectType;
    IntPtr _registerEnumType;
    IntPtr _registerRunLoopCallback;
    IntPtr _unregisterRunLoopCallback;
    IntPtr _registerExitCallback;
    IntPtr _unregisterExitCallback;
    IntPtr _referenceVariable;
    IntPtr _referenceChainVariable;
    IntPtr _releaseVariable;
    IntPtr _setExternalVariable;
    IntPtr _removeExternalVariable;
    IntPtr _allocExternalVariable;
    IntPtr _freeExternalVariable;
    IntPtr _suspend;
    IntPtr _getState;
    IntPtr _abortChain;
    IntPtr _cloneVar;
    IntPtr _destroyVar;
    IntPtr _readCachedString;
    IntPtr _writeCachedString;
    IntPtr _isEqualVar;
    IntPtr _isEqualType;
    IntPtr _deriveTypeInfo;
    IntPtr _freeDerivedTypeInfo;
    IntPtr _seqFree;
    IntPtr _seqPush;
    IntPtr _seqInsert;
    IntPtr _seqPop;
    IntPtr _seqResize;
    IntPtr _seqFastDelete;
    IntPtr _seqSlowDelete;
    IntPtr _typesFree;
    IntPtr _typesPush;
    IntPtr _typesInsert;
    IntPtr _typesPop;
    IntPtr _typesResize;
    IntPtr _typesFastDelete;
    IntPtr _typesSlowDelete;
    IntPtr _paramsFree;
    IntPtr _paramsPush;
    IntPtr _paramsInsert;
    IntPtr _paramsPop;
    IntPtr _paramsResize;
    IntPtr _paramsFastDelete;
    IntPtr _paramsSlowDelete;
    IntPtr _blocksFree;
    IntPtr _blocksPush;
    IntPtr _blocksInsert;
    IntPtr _blocksPop;
    IntPtr _blocksResize;
    IntPtr _blocksFastDelete;
    IntPtr _blocksSlowDelete;
    IntPtr _expTypesFree;
    IntPtr _expTypesPush;
    IntPtr _expTypesInsert;
    IntPtr _expTypesPop;
    IntPtr _expTypesResize;
    IntPtr _expTypesFastDelete;
    IntPtr _expTypesSlowDelete;
    IntPtr _enumsFree;
    IntPtr _enumsPush;
    IntPtr _enumsInsert;
    IntPtr _enumsPop;
    IntPtr _enumsResize;
    IntPtr _enumsFastDelete;
    IntPtr _enumsSlowDelete;
    IntPtr _stringsFree;
    IntPtr _stringsPush;
    IntPtr _stringsInsert;
    IntPtr _stringsPop;
    IntPtr _stringsResize;
    IntPtr _stringsFastDelete;
    IntPtr _stringsSlowDelete;

    delegate byte SuspendDelegate(IntPtr context, double duration);
    SuspendDelegate _suspendDelegate;
    public byte Suspend(IntPtr context, double duration)
    {
      if (_suspendDelegate == null)
      {
        _suspendDelegate = (SuspendDelegate)Marshal.GetDelegateForFunctionPointer(_suspend, typeof(SuspendDelegate));
      }
      return _suspendDelegate(context, duration);
    }

    delegate IntPtr AllocExternalVariableDelegate(IntPtr chain, string name);
    AllocExternalVariableDelegate _allocExternalVariableDelegate;
    public IntPtr AllocExternalVariable(IntPtr chain, string name)
    {
      if (_allocExternalVariableDelegate == null)
      {
        _allocExternalVariableDelegate = (AllocExternalVariableDelegate)Marshal.GetDelegateForFunctionPointer(_allocExternalVariable, typeof(AllocExternalVariableDelegate));
      }
      return _allocExternalVariableDelegate(chain, name);
    }

    delegate IntPtr CreateNodeDelegate();
    CreateNodeDelegate _createNodeDelegate;
    public IntPtr CreateNode()
    {
      if (_createNodeDelegate == null)
      {
        _createNodeDelegate = (CreateNodeDelegate)Marshal.GetDelegateForFunctionPointer(_createNode, typeof(CreateNodeDelegate));
      }
      return _createNodeDelegate();
    }

    delegate byte TickDelegate(IntPtr nodeRef);
    TickDelegate _tickDelegate;
    public byte Tick(IntPtr nodeRef)
    {
      if (_tickDelegate == null)
      {
        _tickDelegate = (TickDelegate)Marshal.GetDelegateForFunctionPointer(_tick, typeof(TickDelegate));
      }
      return _tickDelegate(nodeRef);
    }

    delegate void ScheduleDelegate(IntPtr nodeRef, IntPtr chainRef);
    ScheduleDelegate _scheduleDelegate;
    public void Schedule(IntPtr nodeRef, IntPtr chainRef)
    {
      if (_scheduleDelegate == null)
      {
        _scheduleDelegate = (ScheduleDelegate)Marshal.GetDelegateForFunctionPointer(_schedule, typeof(ScheduleDelegate));
      }
      _scheduleDelegate(nodeRef, chainRef);
    }
  }

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  public delegate CBVar ActivateUnmanagedDelegate(IntPtr context, IntPtr input);

  public static class Native
  {
    static IntPtr _core;
    static bool _coreInitialized;
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
    public static extern CBVar cbLispEval(IntPtr lisp, String code);

    public static ref CBCore Core
    {
      get
      {
        if (!_coreInitialized)
        {
          _core = chainblocksInterface(0x20200101);
          _coreInitialized = true;
          _coreCopy = (CBCore)Marshal.PtrToStructure(_core, typeof(CBCore));
        }
        return ref _coreCopy;
      }
    }
  }
}
