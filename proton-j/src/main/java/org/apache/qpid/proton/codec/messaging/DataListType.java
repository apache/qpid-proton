package org.apache.qpid.proton.codec.messaging;

import java.util.Collection;
import java.util.function.Consumer;

import org.apache.qpid.proton.amqp.messaging.Data;
import org.apache.qpid.proton.amqp.messaging.DataList;
import org.apache.qpid.proton.codec.AMQPType;
import org.apache.qpid.proton.codec.TypeEncoding;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.EncoderImpl;

public class DataListType 
	implements AMQPType<DataList>
{
	private final EncoderImpl _encoder;
	
	private DataListType(final EncoderImpl encoder)
	{
		this._encoder = encoder;
	}

	@Override
	public Class<DataList> getTypeClass()
	{
		return DataList.class;
	}

	@Override
	public TypeEncoding<DataList> getEncoding(DataList val)
	{
		throw new UnsupportedOperationException();
	}

	@Override
	public TypeEncoding<DataList> getCanonicalEncoding()
	{
		return null;
	}

	@Override
	public Collection<? extends TypeEncoding<DataList>> getAllEncodings()
	{
		throw new UnsupportedOperationException();
	}

	@Override
	public void write(DataList val)
	{
		final AMQPType dataType = this._encoder.getTypeFromClass(Data.class);
		val.getValue().forEach(new Consumer<Data>()
		{
			@Override
			public void accept(Data data)
			{
				dataType.write(data);
			}
		});
	}
	
	public static void register(Decoder decoder, EncoderImpl encoder)
	{
		// there is no special Descriptor for DataListType; other than the individual 'DataType';
		// so I believe - no Decoder needs to be registered
		DataListType type = new DataListType(encoder);
		encoder.register(type);
	}
}
