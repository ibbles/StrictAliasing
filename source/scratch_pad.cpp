#include <iostream>
#include <type_traits>

#include <cassert>
#include <cstddef>
#include <cstring>

template<size_t NUM_BYTES = 1024>
class ScratchPad
{
public:
  template<typename T>
  void push(T value)
  {
    static_assert(std::is_trivially_copyable<T>::value, "");
    assert(m_offset + sizeof(T) <= NUM_BYTES);

    memcpy(m_buffer + m_offset, &value, sizeof(T));
    m_offset += sizeof(T);
  }

  template<typename T>
  T pop()
  {
    static_assert(std::is_trivially_copyable<T>::value, "");
    assert(m_offset >= sizeof(T));

    T value;
    m_offset -= sizeof(T);
    memcpy(&value, m_buffer + m_offset, sizeof(T));
    return value;
  }

private:
  size_t m_offset = 0;
  char m_buffer[NUM_BYTES];
};


int get_next_batch_size();
double get_next_value();
void publish(double d);


ScratchPad<> scratch_pad;

void produce()
{
  scratch_pad.push(0);

  int batch_size;
  while ((batch_size = get_next_batch_size()) > 0)
  {
    for (int i = 0; i < batch_size; ++i)
    {
      scratch_pad.push(get_next_value());
    }
    scratch_pad.push(batch_size);
  }
}

void consume()
{
  int batch_size;
  while ((batch_size = scratch_pad.pop<int>()) > 0)
  {
    double sum = 0.0;
    for (int i = 0; i < batch_size; ++i)
    {
      sum += scratch_pad.pop<double>();
    }
    publish(sum);
  }
}


int get_next_batch_size()
{
    static int i = 3;
    return i--;
}



double get_next_value()
{
    static double d = 0.0;
    return d += 1.0;
}


void publish(double d)
{
    std::cout << d << '\n';
}

int main()
{
    produce();
    consume();
}