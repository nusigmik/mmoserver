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
			data_.resize(std::max<size_t>(size, 1));	// 최소 크기 1할당.
		}

		// 복사 생성자
		ByteBuffer(const this_type& rhs)
			: max_capacity_(0)
			, r_index_(0)
			, w_index_(0)
			, data_()
		{
			*this = rhs;
		}

		// 대입 연산자
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

		// 이동 생성자
		ByteBuffer(this_type&& rhs) noexcept
			: max_capacity_(0)
			, r_index_(0)
			, w_index_(0)
			, data_()
		{
			*this = std::move(rhs);
		}

		// 이동 대입 연산자
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

		// 버퍼의 용량.
		size_t Capacity() const
		{
			return data_.size();
		}

		// 버퍼의 용량을 조절 한다.
		void AdjustCapacity(size_t new_capacity)
		{
			if (new_capacity > MaxCapacity())
			{
				throw std::out_of_range("new_capacity exceeds MaxCapacity");
			}

			size_t old_capacity = data_.size();
			// 버퍼 크기를 리사이즈.
			data_.resize(new_capacity);
			// 이전 크기보다 줄어들 경우
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

		// 버퍼 최대 용량.
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

		// 읽을수 있는 크기를 얻는다
		size_t ReadableBytes() const
		{
			return WriterIndex() - ReaderIndex();
		}

		// 쓸수 있는 크기를 얻는다 (new_capacity 까지)
		size_t WritableBytes() const
		{
			return Capacity() - WriterIndex();
		}

		// Max cpapcity 까지 쓸수 있는 크기를 얻는다
		size_t MaxWritableBytes() const
		{
			return MaxCapacity() - WriterIndex();
		}

		// 읽을수 있는 공간이 있는지 검사
		bool IsReadable() const
		{
			return ReadableBytes() > 0;
		}

		// size 만큼 읽을수 있는지 검사
		bool IsReadable(int size) const
		{
			return ReadableBytes() >= size;
		}

		// 쓸수 있는 공간이 있는지 검사
		bool IsWritable() const
		{
			return WritableBytes() > 0;
		}

		// size 만큼 쓸수 있는지 검사
		bool IsWritable(int size) const
		{
			return WritableBytes() >= size;
		}

		// ReaderIndex, WriterIndex 를 0 으로 설정. 내부 데이터를 지우지는 않는다.
		void Clear()
		{
			r_index_ = w_index_ = 0;
		}

		// 이미 읽은 바이트는 버리고 뒤의 읽지 않은 바이트를 앞으로 땡긴다
		void DiscardReadBytes()
		{
			if (r_index_ == 0)
				return;

			if (r_index_ == w_index_)
			{
				r_index_ = w_index_ = 0;
				return;
			}

			// 앞으로 땡긴다
			std::memmove(data_.data(), data_.data() + r_index_, w_index_ - r_index_);
			w_index_ -= r_index_;
			r_index_ = 0;
		}

		// min_writable_bytes 만큼 쓸수 있는 공간을 확보한다.
		void EnsureWritable(size_t min_writable_bytes)
		{
			// 쓸수 있는 공간이 충분
			if (min_writable_bytes <= WritableBytes())
				return;

			// 최대 용량 초과
			if (WriterIndex() + min_writable_bytes > MaxCapacity())
			{
				throw std::out_of_range("Ensure writable bytes is greater than the max capacity");
			}

			// 용량 계산(2배씩 증가)
			size_t new_capacity = CalcNewCapacity(WriterIndex() + min_writable_bytes);
			// 용량 크기를 조절한다.
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
			// POD 타입이 아니면 컴파일 에러
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
			// POD 타입이 아니면 컴파일 에러
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

		// index 위치에 삽입. ReaderIndex, WriterIndex 를 변경하지 않는다. 필요하면 직접 변경 하도록한다.
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

		// POD 타입만 읽는다. ReaderIndex 를 읽은만큼 전진시킨다.
		template <typename PodType>
		void ReadPOD(PodType& dst)
		{
			ReadPOD(&dst);
		}

		// POD 타입만 읽는다. ReaderIndex 를 읽은만큼 전진시킨다.
		template <typename PodType>
		void ReadPOD(PodType* dst)
		{
			constexpr size_t length = sizeof(PodType);
			CheckReadableBytes(length);
			GetPOD(r_index_, dst);
			r_index_ += length;
		}

		// 버퍼에서 length 만큼의 바이트를 읽고 새 버퍼로 리턴.
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

		// bytes 만큼 ReaderIndex 전진 시킨다.
		void SkipBytes(size_t length)
		{
			CheckReadableBytes(length);
			r_index_ += length;
		}

		//// Send the pod array.
		//template <typename PodType, std::size_t N>
		//void Send(const PodType(&src)[N])
		//{
		//	// POD 타입이 아니면 컴파일 에러
		//	static_assert(std::is_pod<PodType>::value, "src is not pod.");

		//	Send((uint8_t*)src, N * sizeof(PodType));
		//}

		//// Send the pod std::array
		//template <typename PodType, std::size_t N>
		//void Send(const std::array<PodType, N>& src)
		//{
		//	// POD 타입이 아니면 컴파일 에러
		//	static_assert(std::is_pod<PodType>::value, "src is not pod.");

		//	Send((uint8_t*)src.data(), src.size() * sizeof(PodType));
		//}

		//// Send the pod std::vector
		//template <typename PodType, typename Allocator>
		//void Send(const std::vector<PodType, Allocator>& src)
		//{
		//	// POD 타입이 아니면 컴파일 에러
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

		// POD 타입만 쓴다. WriterIndex 를 쓴만큼 전진한다.
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

		// 버퍼의 내부 배열을 리턴.
		const uint8_t* Data() const
		{
			return data_.data();
		}

		// 버퍼의 내부 배열을 리턴.
		uint8_t* Data()
		{
			return data_.data();
		}

		// Copy 함수들은 깊은 복사를 한다
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
		// 영역이 유효한지 체크한다.
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

		// readable_bytes 만큼 읽을수 있는지 체크한다
		void CheckReadableBytes(size_t min_readable_bytes) const
		{
			if (r_index_ + min_readable_bytes > w_index_)
			{
				throw std::out_of_range("");
			}
		}

		// 새로 할당할 버퍼 크기를 계산
		size_t CalcNewCapacity(size_t min_new_capacity)
		{
			size_t max_capacity = MaxCapacity();
			// 두배씩 늘인다. 시작 크기는 64.
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

	// 기본 버퍼 타입.
	using Buffer = net::ByteBuffer<>;

} // namespace net