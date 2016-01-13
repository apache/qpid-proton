package org.apache.qpid.proton.amqp.messaging;

import java.util.function.Consumer;

public class DataList 
	implements Section
{
	private final Iterable<Data> _dataList;
	
	public DataList(final Iterable<Data> dataList)
	{
		this._dataList = dataList;
	}
	
	public Iterable<Data> getValue()
	{
		return this._dataList;
	}
	
	@Override
    public String toString()
    {
        final StringBuilder dataListStr = new StringBuilder();
        dataListStr.append("DataList{");
        this._dataList.forEach(new Consumer<Data>()
        {
			@Override
			public void accept(Data data)
			{
				dataListStr.append(data.toString());
			}
		});
        dataListStr.append('}');
        return dataListStr.toString();
    }
}