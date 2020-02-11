#pragma once

#include <iostream>
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <array>
#include <vector>

namespace net {

	template <typename Allocator = std::allocator<uint8_t>>
	class ByteBuffer
	{
	public:
		typedef ByteBuffer<Allocator> this_type;

		explicit ByteBuffer(std::size_t initial_capacity = 1024, std::size_t max_capacity = std::numeric_limits<size_t>::max(), const Allocator& allocator = Allocator())
			: max_capacity_(max_capacity)
			, r_index_(0)
			, w_index_(0)
			, data_(allocator)
		{
			size_t size = std::min<size_t>(max_capacity_, initial_capacity);
			data_.resize(std::max<size_t>(size, 1));	// �ּ� ũ�� 1�Ҵ�.
		}

		// ���� ������
		ByteBuffer(const this_type& rhs)
			: max_capacity_(0)
			, r_index_(0)
			, w_index_(0)
			, data_()
		{
			*this = rhs;
		}

		// ���� ������
		this_type& operator=(const this_type& rhs)
		{
			if (this == &rhs)
				return *this;

			max_capacity_ = rhs.max_capacity_;
			r_index_ = rhs.r_index_;
			w_index_ = rhs.w_index_;
			data_ = rhs.data_;

			return *this;
		}

		// �̵� ������
		ByteBuffer(this_type&& rhs) noexcept
			: max_capacity_(0)
			, r_index_(0)
			, w_index_(0)
			, data_()
		{
			*this = std::move(rhs);
		}

		// �̵� ���� ������
		this_type& operator=(this_type&& rhs) noexcept
		{
			if (this == &rhs)
				return *this;

			max_capacity_ = rhs.max_capacity_;
			r_index_ = rhs.r_index_;
			w_index_ = rhs.w_index_;
			data_ = std::move(rhs.data_);
			rhs.r_index_ = rhs.w_index_ = 0;

			return *this;
		}

		// ������ �뷮.
		size_t Capacity() const
		{
			return data_.size();
		}

		// ������ �뷮�� ���� �Ѵ�.
		void AdjustCapacity(size_t new_capacity)
		{
			if (new_capacity > MaxCapacity())
			{
				throw std::out_of_range("new_capacity exceeds MaxCapacity");
			}

			size_t old_capacity = data_.size();
			// ���� ũ�⸦ ��������.
			data_.resize(new_capacity);
			// ���� ũ�⺸�� �پ�� ���
			if (new_capacity < old_capacity)
			{
				if (ReaderIndex() < new_capacity)
				{
					if (WriterIndex() > new_capacity)
					{
						WriterIndex(new_capacity);
					}
				}
				else
				{
					SetIndex(new_capacity, new_capacity);
				}
			}
		}

		// ���� �ִ� �뷮.
		size_t MaxCapacity() const
		{
			return max_capacity_;
		}

		size_t ReaderIndex() const
		{
			return r_index_;
		}

		size_t WriterIndex() const
		{
			return w_index_;
		}

		void ReaderIndex(size_t index)
		{
			if (index > w_index_)
			{
				throw std::out_of_range("ReaderIndex out of range");
			}
			r_index_ = index;
		}

		void WriterIndex(size_t index)
		{
			if (index < r_index_ || index > Capacity())
			{
				throw std::out_of_range("WriterIndex out of range");
			}
			w_index_ = index;
		}

		// Sets both index
		void SetIndex(size_t reader_index, size_t writer_index)
		{
			if (reader_index > writer_index || writer_index > Capacity())
			{
				throw std::out_of_range("Index out of range");
			}
			r_index_ = reader_index;
			w_index_ = writer_index;
		}

		// ������ �ִ� ũ�⸦ ��´�
		size_t ReadableBytes() const
		{
			return WriterIndex() - ReaderIndex();
		}

		// ���� �ִ� ũ�⸦ ��´� (new_capacity ����)
		size_t WritableBytes() const
		{
			return Capacity() - WriterIndex();
		}

		// Max cpapcity ���� ���� �ִ� ũ�⸦ ��´�
		size_t MaxWritableBytes() const
		{
			return MaxCapacity() - WriterIndex();
		}

		// ������ �ִ� ������ �ִ��� �˻�
		bool IsReadable() const
		{
			return ReadableBytes() > 0;
		}

		// size ��ŭ ������ �ִ��� �˻�
		bool IsReadable(int size) const
		{
			return ReadableBytes() >= size;
		}

		// ���� �ִ� ������ �ִ��� �˻�
		bool IsWritable() const
		{
			return WritableBytes() > 0;
		}

		// size ��ŭ ���� �ִ��� �˻�
		bool IsWritable(int size) const
		{
			return WritableBytes() >= size;
		}

		// ReaderIndex, WriterIndex �� 0 ���� ����. ���� �����͸� �������� �ʴ´�.
		void Clear()
		{
			r_index_ = w_index_ = 0;
		}

		// �̹� ���� ����Ʈ�� ������ ���� ���� ���� ����Ʈ�� ������ �����
		void DiscardReadBytes()
		{
			if (r_index_ == 0)
				return;

			if (r_index_ == w_index_)
			{
				r_index_ = w_index_ = 0;
				return;
			}

			// ������ �����
			std::memmove(data_.data(), data_.data() + r_index_, w_index_ - r_index_);
			w_index_ -= r_index_;
			r_index_ = 0;
		}

		// min_writable_bytes ��ŭ ���� �ִ� ������ Ȯ���Ѵ�.
		void EnsureWritable(size_t min_writable_bytes)
		{
			// ���� �ִ� ������ ���
			if (min_writable_bytes <= WritableBytes())
				return;

			// �ִ� �뷮 �ʰ�
			if (WriterIndex() + min_writable_bytes > MaxCapacity())
			{
				throw std::out_of_range("Ensure writable bytes is greater than the max capacity");
			}

			// �뷮 ���(2�辿 ����)
			size_t new_capacity = CalcNewCapacity(WriterIndex() + min_writable_bytes);
			// �뷮 ũ�⸦ �����Ѵ�.
			AdjustCapacity(new_capacity);
		}

		template <typename PodType>
		void GetPOD(size_t index, PodType& dst) const
		{
			GetPOD(index, &dst);
		}

		template <typename PodType>
		void GetPOD(size_t index, PodType* dst) const
		{
			// POD Ÿ���� �ƴϸ� ������ ����
			static_assert(std::is_pod<PodType>::value, "dst is not pod.");

			//std::memcpy(dst, data_.at(0) + rpos_, length);
			*dst = *((PodType*)(data_.data() + index));
		}

		void GetBytes(size_t index, this_type& dst) const
		{
			GetBytes(index, dst, dst.WritableBytes());
		}

		void GetBytes(size_t index, this_type& dst, size_t length) const
		{
			GetBytes(index, dst, dst.WriterIndex(), length);
		}

		void GetBytes(size_t index, uint8_t* dst, size_t length) const
		{
			GetBytes(index, dst, 0, length);
		}

		void GetBytes(size_t index, this_type& dst, size_t dstIndex, size_t length) const
		{
			CheckIndex(index, length);
			dst.CheckIndex(dstIndex, length);

			GetBytes(index, dst.Data(), dstIndex, length);
		}

		void GetBytes(size_t index, uint8_t* dst, size_t dstIndex, size_t length) const
		{
			CheckIndex(index, length);
			std::memcpy(dst + dstIndex, data_.data() + index, length);
		}

		template <typename PodType>
		void SetPOD(size_t index, const PodType& value)
		{
			SetPOD(index, &value);
		}

		template <typename PodType>
		void SetPOD(size_t index, const PodType* value)
		{
			// POD Ÿ���� �ƴϸ� ������ ����
			static_assert(std::is_pod<PodType>::value, "value is not pod.");

			//std::memcpy(data_.data() + index, value, length);
			*(PodType*)(data_.data() + index) = *value;
		}

		void SetByte(size_t index, const this_type& src)
		{
			SetBytes(index, src, src.ReadableBytes());
		}

		void SetBytes(size_t index, const this_type& src, size_t length)
		{
			SetBytes(index, src, src.ReaderIndex(), length);
		}

		void SetBytes(size_t index, const uint8_t* src, size_t length)
		{
			SetBytes(index, src, 0, length);
		}

		void SetBytes(size_t index, const this_type& src, size_t srcIndex, size_t length)
		{
			CheckIndex(index, length);
			src.CheckIndex(srcIndex, length);

			SetBytes(index, src.Data(), srcIndex, length);
		}

		void SetBytes(size_t index, const uint8_t* src, size_t srcIndex, size_t length)
		{
			CheckIndex(index, length);
			std::memcpy(data_.data() + index, src + srcIndex, length);
		}

		// index ��ġ�� ����. ReaderIndex, WriterIndex �� �������� �ʴ´�. �ʿ��ϸ� ���� ���� �ϵ����Ѵ�.
		void InsertBytes(size_t index, const uint8_t* src, size_t srcIndex, size_t length)
		{
			CheckIndex(index);
			size_t old_cap = Capacity();
			size_t add_length = (index > w_index_) ? (index - w_index_) : 0;
			EnsureWritable(length + add_length);
			std::memmove(Data() + index + length, Data() + index, old_cap - index);
			SetBytes(index, src, srcIndex, length);
		}

		void Read(bool& dst) { ReadPOD(dst); }
		void Read(char& dst) { ReadPOD(dst); }
		void Read(int8_t& dst) { ReadPOD(dst); }
		void Read(uint8_t& dst) { ReadPOD(dst); }
		void Read(int16_t& dst) { ReadPOD(dst); }
		void Read(uint16_t& dst) { ReadPOD(dst); }
		void Read(int32_t& dst) { ReadPOD(dst); }
		void Read(uint32_t& dst) { ReadPOD(dst); }
		void Read(int64_t& dst) { ReadPOD(dst); }
		void Read(uint64_t& dst) { ReadPOD(dst); }
		void Read(float& dst) { ReadPOD(dst); }
		void Read(double& dst) { ReadPOD(dst); }

		void Read(bool* dst) { ReadPOD(dst); }
		void Read(char* dst) { ReadPOD(dst); }
		void Read(int8_t* dst) { ReadPOD(dst); }
		void Read(uint8_t* dst) { ReadPOD(dst); }
		void Read(int16_t* dst) { ReadPOD(dst); }
		void Read(uint16_t* dst) { ReadPOD(dst); }
		void Read(int32_t* dst) { ReadPOD(dst); }
		void Read(uint32_t* dst) { ReadPOD(dst); }
		void Read(int64_t* dst) { ReadPOD(dst); }
		void Read(uint64_t* dst) { ReadPOD(dst); }
		void Read(float* dst) { ReadPOD(dst); }
		void Read(double* dst) { ReadPOD(dst); }

		// POD Ÿ�Ը� �д´�. ReaderIndex �� ������ŭ ������Ų��.
		template <typename PodType>
		void ReadPOD(PodType& dst)
		{
			ReadPOD(&dst);
		}

		// POD Ÿ�Ը� �д´�. ReaderIndex �� ������ŭ ������Ų��.
		template <typename PodType>
		void ReadPOD(PodType* dst)
		{
			constexpr size_t length = sizeof(PodType);
			CheckReadableBytes(length);
			GetPOD(r_index_, dst);
			r_index_ += length;
		}

		// ���ۿ��� length ��ŭ�� ����Ʈ�� �а� �� ���۷� ����.
		this_type ReadBytes(size_t length)
		{
			CheckReadableBytes(length);
			if (length == 0)
			{
				return this_type(0, 0);
			}

			this_type buf(length, MaxCapacity());
			buf.WriteBytes(*this, ReaderIndex(), length);
			r_index_ += length;

			return std::move(buf);
		}

		void ReadBytes(this_type& dst)
		{
			ReadBytes(dst, dst.WritableBytes());
		}

		void ReadBytes(this_type& dst, size_t length)
		{
			if (length > dst.WritableBytes())
			{
				throw std::out_of_range("length exceeds dst.WritableBytes");
			}
			ReadBytes(dst, dst.WriterIndex(), length);
			dst.WriterIndex(dst.WriterIndex() + length);
		}

		void ReadBytes(this_type& dst, size_t dstIndex, size_t length)
		{
			CheckReadableBytes(length);
			GetBytes(ReaderIndex(), dst, dstIndex, length);
			r_index_ += length;
		}

		void ReadBytes(uint8_t* dst, size_t length)
		{
			ReadBytes(dst, 0, length);
		}

		void ReadBytes(uint8_t* dst, size_t dstIndex, size_t length)
		{
			CheckReadableBytes(length);
			GetBytes(ReaderIndex(), dst, dstIndex, length);
			r_index_ += length;
		}

		// bytes ��ŭ ReaderIndex ���� ��Ų��.
		void SkipBytes(size_t length)
		{
			CheckReadableBytes(length);
			r_index_ += length;
		}

		//// Send the pod array.
		//template <typename PodType, std::size_t N>
		//void Send(const PodType(&src)[N])
		//{
		//	// POD Ÿ���� �ƴϸ� ������ ����
		//	static_assert(std::is_pod<PodType>::value, "src is not pod.");

		//	Send((uint8_t*)src, N * sizeof(PodType));
		//}

		//// Send the pod std::array
		//template <typename PodType, std::size_t N>
		//void Send(const std::array<PodType, N>& src)
		//{
		//	// POD Ÿ���� �ƴϸ� ������ ����
		//	static_assert(std::is_pod<PodType>::value, "src is not pod.");

		//	Send((uint8_t*)src.data(), src.size() * sizeof(PodType));
		//}

		//// Send the pod std::vector
		//template <typename PodType, typename Allocator>
		//void Send(const std::vector<PodType, Allocator>& src)
		//{
		//	// POD Ÿ���� �ƴϸ� ������ ����
		//	static_assert(std::is_pod<PodType>::value, "src is not pod.");

		//	Send((uint8_t*)src.data(), src.size() * sizeof(PodType));
		//}

		//// Send the std::string
		//template <typename Elem, typename Traits, typename Allocator>
		//void Send(const std::basic_string<Elem, Traits, Allocator>& src)
		//{
		//	Send((uint8_t*)src.data(), src.size() * sizeof(Elem));
		//}

		void Write(const bool& value) { WritePOD(value); }
		void Write(const char& value) { WritePOD(value); }
		void Write(const int8_t& value) { WritePOD(value); }
		void Write(const uint8_t& value) { WritePOD(value); }
		void Write(const int16_t& value) { WritePOD(value); }
		void Write(const uint16_t& value) { WritePOD(value); }
		void Write(const int32_t& value) { WritePOD(value); }
		void Write(const uint32_t& value) { WritePOD(value); }
		void Write(const int64_t& value) { WritePOD(value); }
		void Write(const uint64_t& value) { WritePOD(value); }
		void Write(const float& value) { WritePOD(value); }
		void Write(const double& value) { WritePOD(value); }

		void Write(const bool* value) { WritePOD(value); }
		void Write(const char* value) { WritePOD(value); }
		void Write(const int8_t* value) { WritePOD(value); }
		void Write(const uint8_t* value) { WritePOD(value); }
		void Write(const int16_t* value) { WritePOD(value); }
		void Write(const uint16_t* value) { WritePOD(value); }
		void Write(const int32_t* value) { WritePOD(value); }
		void Write(const uint32_t* value) { WritePOD(value); }
		void Write(const int64_t* value) { WritePOD(value); }
		void Write(const uint64_t* value) { WritePOD(value); }
		void Write(const float* value) { WritePOD(value); }
		void Write(const double* value) { WritePOD(value); }

		// POD Ÿ�Ը� ����. WriterIndex �� ����ŭ �����Ѵ�.
		template <typename PodType>
		void WritePOD(const PodType& src)
		{
			WritePOD(&src);
		}

		template <typename PodType>
		void WritePOD(const PodType* src)
		{
			constexpr size_t length = sizeof(PodType);
			EnsureWritable(length);
			SetPOD(w_index_, src);
			w_index_ += length;
		}

		void WriteBytes(this_type& src)
		{
			WriteBytes(src, src.ReadableBytes());
		}

		void WriteBytes(this_type& src, size_t length)
		{
			if (length > src.ReadableBytes())
			{
				throw std::out_of_range("length exceeds src.ReadableBytes");
			}
			WriteBytes(src, src.ReaderIndex(), length);
			src.ReaderIndex(src.ReaderIndex() + length);
		}

		void WriteBytes(const this_type& src, size_t srcIndex, size_t length)
		{
			EnsureWritable(length);
			SetBytes(w_index_, src, srcIndex, length);
			w_index_ += length;
		}

		void WriteBytes(const uint8_t* src, size_t length)
		{
			WriteBytes(src, 0, length);
		}

		void WriteBytes(const uint8_t* src, size_t srcIndex, size_t length)
		{
			EnsureWritable(length);
			SetBytes(w_index_, src, srcIndex, length);
			w_index_ += length;
		}

		// ������ ���� �迭�� ����.
		const uint8_t* Data() const
		{
			return data_.data();
		}

		// ������ ���� �迭�� ����.
		uint8_t* Data()
		{
			return data_.data();
		}

		// Copy �Լ����� ���� ���縦 �Ѵ�
		this_type Copy() const
		{
			return Copy(ReaderIndex(), ReadableBytes());
		}

		this_type Copy(size_t index, size_t length) const
		{
			CheckIndex(index, length);

			this_type new_buffer(length, MaxCapacity(), data_.get_allocator());
			std::memcpy(new_buffer.Data(), Data() + index, length);
			new_buffer.WriterIndex(length);

			return std::move(new_buffer);
		}

	protected:
		// ������ ��ȿ���� üũ�Ѵ�.
		void CheckIndex(size_t index) const
		{
			if (index >= Capacity())
			{
				throw std::out_of_range("");
			}
		}

		void CheckIndex(size_t index, size_t length) const
		{
			if (index + length > Capacity())
			{
				throw std::out_of_range("");
			}
		}

		// readable_bytes ��ŭ ������ �ִ��� üũ�Ѵ�
		void CheckReadableBytes(size_t min_readable_bytes) const
		{
			if (r_index_ + min_readable_bytes > w_index_)
			{
				throw std::out_of_range("");
			}
		}

		// ���� �Ҵ��� ���� ũ�⸦ ���
		size_t CalcNewCapacity(size_t min_new_capacity)
		{
			size_t max_capacity = MaxCapacity();
			// �ι辿 ���δ�. ���� ũ��� 64.
			size_t new_capacity = 64;
			while (new_capacity < min_new_capacity)
			{
				new_capacity <<= 1;
			}

			return std::min<size_t>(new_capacity, max_capacity);
		}

		size_t r_index_, w_index_;
		size_t max_capacity_;
		std::vector<uint8_t> data_;
	};

	// �⺻ ���� Ÿ��.
	using Buffer = net::ByteBuffer<>;

} // namespace net