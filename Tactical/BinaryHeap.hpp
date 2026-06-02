

#ifndef BINARY_HEAP_HPP
#define BINARY_HEAP_HPP

#include <vector>

template <typename KEY = int, class T = int>
class HEAP
{
public:
	KEY	key;
	T	data;

	HEAP<KEY, T>(void){return;};
	HEAP<KEY, T>(T data, int key) {HEAP::data = data; HEAP::key = key; return;};

	bool operator==(HEAP<KEY, T> &compare) {
		return (this->key == compare.key && this->data == compare.data);
	}
};

// CBinaryHeap. With TrackIndex == true the heap also maintains a data -> heap-index
// map (m_index, keyed by (int)data) so findData(data) is O(1) instead of an O(n)
// linear scan -- this turns the A* decrease-key (editElement) from O(n) into
// O(log n), the single biggest tactical-pathfinding win on open maps.
//
// It is opt-in: TrackIndex defaults to false, so heaps over non-integer data
// (e.g. the strategic CSmallPoint pathfinder) compile and run byte-for-byte as
// before and pay nothing (every map update is behind `if constexpr (TrackIndex)`,
// so it isn't even instantiated, and no std::hash<T> is required).
//
// When enabled the caller must guarantee: (a) (int)data is a stable index in
// [0, maxSize] -- true for the tactical gridno heap (gridno < WORLD_MAX == maxSize);
// and (b) each data value is in the heap at most once (the A* invariant: a node is
// inserted once, then decrease-key'd in place). m_index[data]==0 means "not in the
// heap" (heap slot 0 is unused), matching findData's not-found return of 0.
template <typename KEY = int, class T = int, bool TrackIndex = false>
class CBinaryHeap
{
public:
	typedef HEAP<KEY, T> CBinaryHeap_t;

	void clear ()
	{
		if constexpr (TrackIndex)
		{
			// Only the entries currently in the heap are non-zero, so reset just
			// those -- O(size), not O(maxSize) -- leaving m_index all-zero again.
			for (int i = 1; i < heapCount; ++i)
				m_index[(int)BinaryHeap[i].data] = 0;
		}
		heapCount = 1;
		return;
	}

	CBinaryHeap(int size=WORLD_MAX)
	{
		if (size <= 0) {
			size = WORLD_MAX;
		}
		//size must be maxSize + 1, because 1 element is unused.
		BinaryHeap = new HEAP<KEY, T>[size+1];
		maxSize = size;
		heapCount = 1;
		if constexpr (TrackIndex) {
			m_index.assign(size+1, 0);
		}
		return;
	}

	~CBinaryHeap()
	{
		delete[] BinaryHeap;
	}

	CBinaryHeap_t removeElement(const T data)
	{
		HEAP<KEY, T> returnHeap;
		int index = findData(data);
		if (index) {
			returnHeap = BinaryHeap[index];
			--heapCount;
			placeAt( moveDown(index, BinaryHeap[heapCount].key), BinaryHeap[heapCount] );
			forget(data);
		}
		return (returnHeap);
	}

	CBinaryHeap_t removeElement(const T data, const KEY key)
	{
		HEAP<KEY, T> returnHeap;
		int index = findData(data, key);
		if (index) {
			returnHeap = BinaryHeap[index];
			--heapCount;
			placeAt( moveDown(index, BinaryHeap[heapCount].key), BinaryHeap[heapCount] );
			forget(data);
		}
		return (returnHeap);
	}

	int moveUp(int index, KEY newKey)
	{
		while (index > 1) {
			int current2 = index>>1;//divided by 2
			if (BinaryHeap[current2].key > newKey) {
				//move parent down
				placeAt(index, BinaryHeap[current2]);
				index = current2;
			}
			else {
				break;
			}
		}
		return (index);
	}

	int moveDown(int index, KEY currentKey)
	{
		int current2 = index+index;
			while (current2 < heapCount) {
			if (current2+1 < heapCount) {
				//choose child to possibly move
				current2 += (BinaryHeap[current2].key > BinaryHeap[current2+1].key);
				if (currentKey > BinaryHeap[current2].key) {
					//move child up
					placeAt(index, BinaryHeap[current2]);
					index = current2;
					current2 += current2;//times 2
				}
				else {
					return index;
				}
			}
			else {
				if (currentKey > BinaryHeap[current2].key) {
					placeAt(index, BinaryHeap[current2]);
					index =  current2;
				}
				return index;
			}
		}
		return index;
	}

	bool editElement(const T oldData, const T newData, const KEY oldKey, const KEY newKey)
	{
		int index = findData(oldData, oldKey);
		if (index) {
			if (oldKey < newKey) {
				index = moveDown(index, newKey);
			}
			else if (oldKey > newKey) {
				index = moveUp(index, newKey);
			}
			// reuse placeAt so the index map tracks both the (possible) data change
			// and the slot it ended up in.
			if constexpr (TrackIndex) {
				if (newData != oldData) m_index[(int)oldData] = 0;
			}
			placeAt(index, newData, newKey);
			return true;
		}
		return false;
	}

	bool editElement(const T data, const KEY key)
	{
		int index = findData(data);
		if (index) {
			int oldKey = BinaryHeap[index].key;
			if (oldKey < key) {
				index = moveDown(index, key);
				placeAt(index, data, key);
			}
			else if (oldKey > key) {
				index = moveUp(index, key);
				placeAt(index, data, key);
			}
			return true;
		}
		return false;
	}

	int insertElement(const T data, const KEY key)
	{
		if (heapCount > maxSize) {
			return 0;
		}
		int index = heapCount;
		while (index > 1) {
			int current2 = index>>1;//divided by 2
			if (BinaryHeap[current2].key > key) {
				//move parent down
				placeAt(index, BinaryHeap[current2]);
				index = current2;
			}
			else {
				break;
			}
		}
		placeAt(index, data, key);
		return (heapCount++);
	}

	CBinaryHeap_t popTopHeap(int& returnSize)
	{
		returnSize = heapCount-1;
		if (heapCount != 1) {
			return (popTopHeap());
		}
		return (BinaryHeap[0]);
	}

	CBinaryHeap_t popTopHeap()
	{
		HEAP<KEY, T> returnHeap;
		if (heapCount != 1) {
			returnHeap = BinaryHeap[1];
			--heapCount;
			KEY currentKey = BinaryHeap[heapCount].key;
			int current2 = 2;
			int index = 1;
			while (current2 < heapCount) {
				if (current2+1 < heapCount) {
					current2 += (BinaryHeap[current2].key > BinaryHeap[current2+1].key);
					//choose child to possibly move
					if (currentKey > BinaryHeap[current2].key) {
						//move child up
						placeAt(index, BinaryHeap[current2]);
						index = current2;
						current2 += current2;//times 2
					}
					else {
						break;
					}
				}
				else {
					if (currentKey > BinaryHeap[current2].key) {
						placeAt(index, BinaryHeap[current2]);
						index =  current2;
					}
					break;
				}
			}
			placeAt(index, BinaryHeap[heapCount]);
			// remove the popped element LAST: if the heap is now empty the line
			// above re-stamped its data at slot 1, so clear it after.
			forget(returnHeap.data);
		}
		return (returnHeap);
	}

	CBinaryHeap_t peekTopHeap() const
	{
		return (BinaryHeap[(heapCount != 1)]);
	}

	CBinaryHeap_t peekElement(int index) const
	{
		if (index < heapCount) {
			return (BinaryHeap[index]);
		}
		else {
			return (BinaryHeap[0]);
		}
	}

	int size() const
	{
		return (heapCount-1);
	}

	int getMaxSize() const
	{
		return (maxSize);
	}

	int findData(const T data) const
	{
		if constexpr (TrackIndex) {
			// O(1): the index map gives the heap slot directly (0 == not present).
			return m_index[(int)data];
		}
		int current;
		for (current = heapCount-1; current > 0; --current) {
			if (BinaryHeap[current].data == data) {
				break;
			}
		}
		return current;
	}

	int findData(const T data, const KEY key) const
	{
		int current;
		if ((BinaryHeap[1].key + BinaryHeap[heapCount-1].key)>>1 > key) {
			for (current = 1; current <= heapCount-1; ++current) {
				if (BinaryHeap[current].data == data && BinaryHeap[current].key == key) {
					return current;
				}
			}
		}
		for (current = heapCount-1; current > 0; --current) {
			if (BinaryHeap[current].data == data && BinaryHeap[current].key == key) {
				break;
			}
		}
		return current;
	}

private:
	// Write an element into a heap slot, keeping the index map in sync. All slot
	// writes go through placeAt so m_index can never drift from the heap.
	inline void placeAt(int index, const CBinaryHeap_t& elem)
	{
		BinaryHeap[index] = elem;
		if constexpr (TrackIndex) m_index[(int)elem.data] = index;
	}
	inline void placeAt(int index, const T& data, const KEY& key)
	{
		BinaryHeap[index].data = data;
		BinaryHeap[index].key  = key;
		if constexpr (TrackIndex) m_index[(int)data] = index;
	}
	inline void forget(const T& data)
	{
		if constexpr (TrackIndex) m_index[(int)data] = 0;
	}

	int				heapCount;
	int				maxSize;
	CBinaryHeap_t*	BinaryHeap;
	std::vector<int> m_index;   // data -> heap index, only used when TrackIndex
};

#endif
