#include "CommandQueue.h"

CommandQueue::CommandQueue(Device device, D3D12_COMMAND_LIST_TYPE type)
	: m_Device(device), m_CommandListType(type), m_FenceValue(0)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(m_Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_CommandQueue)));
	ThrowIfFailed(m_Device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));

	m_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(m_FenceEvent && "Failed to create fence event handle.");
}

GraphicsCommandList CommandQueue::GetFreeCommandList()
{
	CommandAllocator commandAllocator;
	GraphicsCommandList commandList;

	if (!m_CommandAllocatorQueue.empty() && IsFenceComplete(m_CommandAllocatorQueue.front().FenceValue))
	{
		commandAllocator = m_CommandAllocatorQueue.front().CommandAllocator;
		m_CommandAllocatorQueue.pop();

		ThrowIfFailed(commandAllocator->Reset());
	}
	else
	{
		commandAllocator = CreateCommandAllocator();
	}

	if (!m_CommandListQueue.empty())
	{
		commandList = m_CommandListQueue.front();
		m_CommandListQueue.pop();

		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}
	else
	{
		commandList = CreateCommandList(commandAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}

UINT64 CommandQueue::ExecuteCommandList(GraphicsCommandList commandList)
{
	commandList->Close();

	ID3D12CommandAllocator *commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList *const ppCommandLists[] = {
		commandList.Get()};

	m_CommandQueue->ExecuteCommandLists(1, ppCommandLists);
	UINT64 fenceValue = Signal();

	m_CommandAllocatorQueue.emplace(CommandAllocatorEntry{fenceValue, commandAllocator});
	m_CommandListQueue.push(commandList);

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference
	// in this temporary COM pointer here.
	commandAllocator->Release();

	return fenceValue;
}

UINT64 CommandQueue::Signal()
{
	UINT64 fenceValue = ++m_FenceValue;
	m_CommandQueue->Signal(m_Fence.Get(), fenceValue);
	return fenceValue;
}

bool CommandQueue::IsFenceComplete(UINT64 fenceValue)
{
	return m_Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(UINT64 fenceValue)
{
	if (!IsFenceComplete(fenceValue))
	{
		m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);
		::WaitForSingleObject(m_FenceEvent, DWORD_MAX);
	}
}

void CommandQueue::Flush()
{
	WaitForFenceValue(Signal());
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetCommandQueue() const
{
	return m_CommandQueue;
}

CommandAllocator CommandQueue::CreateCommandAllocator()
{
	CommandAllocator commandAllocator;
	ThrowIfFailed(m_Device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

GraphicsCommandList CommandQueue::CreateCommandList(CommandAllocator allocator)
{
	GraphicsCommandList commandList;
	ThrowIfFailed(m_Device->CreateCommandList(0, m_CommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	return commandList;
}