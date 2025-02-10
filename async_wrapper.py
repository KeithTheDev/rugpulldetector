import asyncio
from functools import partial
from typing import Dict
from .rugpull_detector import check_rug_pull_sync


async def check_rug_pull(
    mint_address: str, redis_url: str = "redis://localhost"
) -> Dict:
    """
    Async wrapper for the rug pull detector.
    """
    loop = asyncio.get_running_loop()
    try:
        # Run sync function in a thread pool
        result = await loop.run_in_executor(
            None, partial(check_rug_pull_sync, mint_address, redis_url)
        )
        return result
    except Exception as e:
        return {"rug_pulled": False, "timestamp": None, "debug_info": {"error": str(e)}}
