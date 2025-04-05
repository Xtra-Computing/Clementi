#ifndef __STREAM_OPERATION__
#define __STREAM_OPERATION__

#if 0
template <typename T>
inline int write_to_stream (hls::stream<T> &stream, T const& value)
{
#pragma HLS INLINE
    int count = 0;
    stream << value;
    return 0;
}

template <typename T>
inline int read_from_stream (hls::stream<T> &stream, T & value)
{
#pragma HLS INLINE
    value = stream.read();
    return 0;
}
#endif

template <typename T>
void sink_stream(hls::stream<T> &stream)
{
#pragma HLS interface ap_ctrl_none port=return

//sink: for (int i = 0 ; i < size ; i++) {
    while (1) {
#pragma HLS PIPELINE II=1
        T data;
        read_from_stream(stream, data);
    }
}

template <typename T>
void dummy_stream(hls::stream<T> &stream)
{
#pragma HLS interface ap_ctrl_none port=return
//sink: for (int i = 0 ; i < size ; i++) {
    while (1) {
#pragma HLS PIPELINE II=1
        T data;
        data.data = 0;
        write_to_stream(stream, data);
    }
}

template <typename T>
void pad(hls::stream<T> &in, hls::stream<T> &out)
{
#pragma HLS interface ap_ctrl_none port=return
    while (1) {
        T data;
        read_from_stream(in, data);
        write_to_stream(out, data);
    }
}

template <typename T>
void stream_duplicator(hls::stream<T> &in, hls::stream<T> &out1, hls::stream<T> &out2)
{
#pragma HLS interface ap_ctrl_none port=return
    while (1) {
        T data;
        read_from_stream(in, data);
        write_to_stream(out1, data);
        write_to_stream(out2, data);
    }
}

template <int T>
void axis_to_hls_stream( hls::stream< ap_axiu<T, 0, 0, 0> > &in, hls::stream< ap_uint<T> > &out )
{
#pragma HLS interface ap_ctrl_none port=return
    while (1) {
        ap_axiu<T, 0, 0, 0>  sdata = in.read();
        out.write(sdata.data);
    }
}


template <int T>
void hls_to_axis_stream( hls::stream< ap_uint<T> > &in, hls::stream< ap_axiu<T, 0, 0, 0> > &out)
{
#pragma HLS interface ap_ctrl_none port=return
    while (1) {
        ap_uint<T> data = in.read();
        ap_axiu<T, 0, 0, 0>  pkg;
        pkg.data = data;
        out.write(pkg);
    }
}

#define FRP_TEMPLATE(name)    name##_stream_t, name##_frp_inner_stream_t, name##_pkg_t, name##_frp_item_t, name##_t




template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void frp_sink_stream(frp_stream_t &stream)
{

    while (1) {
#pragma HLS PIPELINE II=1
        frp_t data;

        if (stream.read_nb(data))
        {
            if (data.ap_frp_last(struct_t) == 1)
            {
                return;
            }
        }
    }
}


template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void frp_pad_stream(frp_stream_t &in,  frp_stream_t &out)
{

    while (1) {
#pragma HLS PIPELINE II=1
        frp_t data;

        if (in.read_nb(data))
        {
            out.write(data);
            if (data.ap_frp_last(struct_t) == 1)
            {
                return;
            }
        }
    }
}



template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void axis_to_frp_stream(stream_t           &axis,
                        frp_stream_t       &frp )
{
frp: while (true)
    {
        pkg_t axis_pkg = axis.read();
        frp_t  frp_item;
        frp_item.ap_frp_data(struct_t) = axis_pkg.data;
        frp_item.ap_frp_last(struct_t) = axis_pkg.last;
        frp.write(frp_item);
        if (axis_pkg.last == 1)
        {
            return;
        }
    }
}

template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void frp_to_axis_stream(frp_stream_t       &frp,
                        stream_t           &axis)
{
frp: while (true)
    {
        pkg_t axis_pkg;
        frp_t  frp_item  = frp.read();
        axis_pkg.data = frp_item.ap_frp_data(struct_t);
        axis_pkg.last = frp_item.ap_frp_last(struct_t);
        axis.write(axis_pkg);
        if (frp_item.ap_frp_last(struct_t) == 1)
        {
            return;
        }
    }
}



template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void free_run_axis_to_frp_stream(   stream_t           &axis,
                                    frp_stream_t       &frp )
{
#pragma HLS interface ap_ctrl_none port=return
frp: while (true)
    {
        pkg_t axis_pkg = axis.read();
        frp_t  frp_item;
        frp_item.ap_frp_data(struct_t) = axis_pkg.data;
        frp_item.ap_frp_last(struct_t) = axis_pkg.last;
        frp.write(frp_item);
    }
}

template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void free_run_frp_to_axis_stream(   frp_stream_t       &frp,
                                    stream_t           &axis)
{
#pragma HLS interface ap_ctrl_none port=return
frp: while (true)
    {
        pkg_t axis_pkg;
        frp_t  frp_item  = frp.read();
        axis_pkg.data = frp_item.ap_frp_data(struct_t);
        axis_pkg.last = frp_item.ap_frp_last(struct_t);
        axis.write(axis_pkg);
    }
}


template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void free_run_frp_sink_stream(frp_stream_t &stream)
{
#pragma HLS interface ap_ctrl_none port=return
    while (1) {
#pragma HLS PIPELINE II=1
        frp_t data;
        data = stream.read();
    }
}


template <typename stream_t, typename frp_stream_t, typename pkg_t, typename frp_t, typename struct_t>
void free_run_frp_pad_stream(frp_stream_t &in,  frp_stream_t &out)
{
#pragma HLS interface ap_ctrl_none port=return
    while (1) {
#pragma HLS PIPELINE II=1
        frp_t data = in.read();
        out.write(data);
    }
}


#endif /*  __STREAM_OPERATION__ */
