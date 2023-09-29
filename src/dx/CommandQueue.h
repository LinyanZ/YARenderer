#pragma once

#include "dx.h"

class CommandQueue
{
public:
	CommandQueue(Device device, D3D12_COMMAND_LIST_TYPE type);

	GraphicsCommandList GetFreeCommandList();
	UINT64 ExecuteCommandList(GraphicsCommandList commandList);

	UINT64 Signal();
	bool IsFenceComplete(UINT64 fenceValue);
	void WaitForFenceValue(UINT64 fenceValue);
	void Flush();

	ComPtr<ID3D12CommandQueue> GetCommandQueue() const;

private:
	CommandAllocator CreateCommandAllocator();
	GraphicsCommandList CreateCommandList(CommandAllocator allocator);

private:
	struct CommandAllocatorEntry
	{
		UINT64 FenceValue;
		CommandAllocator CommandAllocator;
	};

	using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
	using CommandListQueue = std::queue<GraphicsCommandList>;

	D3D12_COMMAND_LIST_TYPE     m_CommandListType;
	Device					m_Device;
	ComPtr<ID3D12CommandQueue>  m_CommandQueue;
	Fence					    m_Fence;
	HANDLE                      m_FenceEvent;
	UINT64                      m_FenceValue;

	CommandAllocatorQueue       m_CommandAllocatorQueue;
	CommandListQueue            m_CommandListQueue;
};